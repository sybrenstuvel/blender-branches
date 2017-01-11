/*
 * Copyright 2011-2016 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

CCL_NAMESPACE_BEGIN

ccl_device void kernel_filter_construct_transform(KernelGlobals *kg, int sample, float ccl_readonly_ptr buffer, int x, int y, FilterStorage *storage, int4 rect)
{
	float features[DENOISE_FEATURES];

	int buffer_w = align_up(rect.z - rect.x, 4);
	int buffer_h = (rect.w - rect.y);
	int pass_stride = buffer_h * buffer_w * kernel_data.film.num_frames;
	int num_frames = kernel_data.film.num_frames;
	int prev_frames = kernel_data.film.prev_frames;

	/* Temporary storage, used in different steps of the algorithm. */
	float tempmatrix[DENOISE_FEATURES*DENOISE_FEATURES];
	float tempvector[2*DENOISE_FEATURES];
	float ccl_readonly_ptr pixel_buffer;
	int3 pixel;




	/* === Calculate denoising window. === */
	int2 low  = make_int2(max(rect.x, x - kernel_data.integrator.half_window),
	                      max(rect.y, y - kernel_data.integrator.half_window));
	int2 high = make_int2(min(rect.z, x + kernel_data.integrator.half_window + 1),
	                      min(rect.w, y + kernel_data.integrator.half_window + 1));




	/* === Shift feature passes to have mean 0. === */
	float feature_means[DENOISE_FEATURES];
	math_vector_zero(feature_means, DENOISE_FEATURES);
	FOR_PIXEL_WINDOW {
		filter_get_features(pixel, pixel_buffer, features, NULL, pass_stride);
		math_vector_add(feature_means, features, DENOISE_FEATURES);
	} END_FOR_PIXEL_WINDOW

	float pixel_scale = 1.0f / ((high.y - low.y) * (high.x - low.x));
	math_vector_scale(feature_means, pixel_scale, DENOISE_FEATURES);

	/* === Scale the shifted feature passes to a range of [-1; 1], will be baked into the transform later. === */
	float *feature_scale = tempvector;
	math_vector_zero(feature_scale, DENOISE_FEATURES);

	FOR_PIXEL_WINDOW {
		filter_get_feature_scales(pixel, pixel_buffer, features, feature_means, pass_stride);
		math_vector_max(feature_scale, features, DENOISE_FEATURES);
	} END_FOR_PIXEL_WINDOW

	filter_calculate_scale(feature_scale);


	/* === Generate the feature transformation. ===
	 * This transformation maps the DENOISE_FEATURES-dimentional feature space to a reduced feature (r-feature) space
	 * which generally has fewer dimensions. This mainly helps to prevent overfitting. */
	float* feature_matrix = tempmatrix, feature_matrix_norm = 0.0f;
	math_trimatrix_zero(feature_matrix, DENOISE_FEATURES);
#ifdef FULL_EIGENVALUE_NORM
	float perturbation_matrix[DENOISE_FEATURES*DENOISE_FEATURES];
	math_trimatrix_zero(perturbation_matrix, NORM_FEATURE_NUM);
#endif
	FOR_PIXEL_WINDOW {
		filter_get_features(pixel, pixel_buffer, features, feature_means, pass_stride);
		math_vector_mul(features, feature_scale, DENOISE_FEATURES);
		math_trimatrix_add_gramian(feature_matrix, DENOISE_FEATURES, features, 1.0f);

		filter_get_feature_variance(pixel_buffer, features, feature_scale, pass_stride);
#ifdef FULL_EIGENVALUE_NORM
		math_trimatrix_add_gramian(perturbation_matrix, NORM_FEATURE_NUM, features, kernel_data.integrator.filter_strength);
#else
		for(int i = 0; i < NORM_FEATURE_NUM; i++)
			feature_matrix_norm += features[i + NORM_FEATURE_OFFSET]*kernel_data.integrator.filter_strength;
#endif
	} END_FOR_PIXEL_WINDOW

	float *feature_transform = &storage->transform[0];
	int rank = math_trimatrix_jacobi_eigendecomposition(feature_matrix, feature_transform, DENOISE_FEATURES, 1);

#ifdef FULL_EIGENVALUE_NORM
	float tempvector_2[2*DENOISE_FEATURES];
	for(int i = 0; i < DENOISE_FEATURES; i++)
		tempvector_2[i] = 1.0f;
	float singular_threshold = 0.01f + 2.0f * sqrtf(math_largest_eigenvalue(perturbation_matrix, NORM_FEATURE_NUM, tempvector_2, tempvector_2 + DENOISE_FEATURES));
#else
	float singular_threshold = 0.01f + 2.0f * (sqrtf(feature_matrix_norm) / (sqrtf(rank) * 0.5f));
	singular_threshold *= singular_threshold;
#endif

	rank = 0;
	for(int i = 0; i < DENOISE_FEATURES; i++, rank++) {
		float s = feature_matrix[i*DENOISE_FEATURES+i];
		if(i >= 2 && s < singular_threshold)
			break;
		/* Bake the feature scaling into the transformation matrix. */
		math_vector_mul(feature_transform + rank*DENOISE_FEATURES, feature_scale, DENOISE_FEATURES);
	}

#ifdef WITH_CYCLES_DEBUG_FILTER
	storage->feature_matrix_norm = feature_matrix_norm;
	storage->singular_threshold = singular_threshold;
	for(int i = 0; i < DENOISE_FEATURES; i++) {
		storage->means[i] = feature_means[i];
		storage->scales[i] = feature_scale[i];
		storage->singular[i] = feature_matrix[i*DENOISE_FEATURES+i];
	}
#endif
	storage->rank = rank;
}

CCL_NAMESPACE_END