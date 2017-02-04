/*
 * Copyright 2011-2017 Blender Foundation
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

#define ccl_get_feature(pass) buffer[(pass)*pass_stride]

/* Loop over the pixels in the range [low.x, high.x) x [low.y, high.y).
 * pixel_buffer always points to the current pixel in the first pass. */
#ifdef DENOISE_TEMPORAL
#define FOR_PIXEL_WINDOW     pixel_buffer = buffer + (low.y - rect.y)*buffer_w + (low.x - rect.x); \
                             for(int t = 0; t < num_frames; t++) { \
                                 pixel.z = (t == 0)? 0: ((t <= prev_frames)? (t-prev_frames-1): (t - prev_frames)); \
	                             for(pixel.y = low.y; pixel.y < high.y; pixel.y++) { \
	                                 for(pixel.x = low.x; pixel.x < high.x; pixel.x++, pixel_buffer++) {

#define END_FOR_PIXEL_WINDOW         } \
                                     pixel_buffer += buffer_w - (high.x - low.x); \
                                 } \
                                 pixel_buffer += buffer_w * (buffer_h - (high.y - low.y)); \
                             }
#else
#define FOR_PIXEL_WINDOW     pixel_buffer = buffer + (low.y - rect.y)*buffer_w + (low.x - rect.x); \
                             for(pixel.y = low.y; pixel.y < high.y; pixel.y++) { \
                                 for(pixel.x = low.x; pixel.x < high.x; pixel.x++, pixel_buffer++) {

#define END_FOR_PIXEL_WINDOW     } \
                                 pixel_buffer += buffer_w - (high.x - low.x); \
                             }
#endif

ccl_device_inline void filter_get_features(int3 pixel, float ccl_readonly_ptr buffer, float *features, float ccl_readonly_ptr mean, int pass_stride)
{
	float *feature = features;
	*(feature++) = pixel.x;
	*(feature++) = pixel.y;
#ifdef DENOISE_TEMPORAL
	*(feature++) = pixel.z;
#endif
	*(feature++) = ccl_get_feature(0);
	*(feature++) = ccl_get_feature(1);
	*(feature++) = ccl_get_feature(2);
	*(feature++) = ccl_get_feature(3);
	*(feature++) = ccl_get_feature(4);
	*(feature++) = ccl_get_feature(5);
	*(feature++) = ccl_get_feature(6);
	*(feature++) = ccl_get_feature(7);
	if(mean) {
		for(int i = 0; i < DENOISE_FEATURES; i++)
			features[i] -= mean[i];
	}
#ifdef DENOISE_SECOND_ORDER_SCREEN
	features[10] = features[0]*features[0];
	features[11] = features[1]*features[1];
	features[12] = features[0]*features[1];
#endif
}

ccl_device_inline void filter_get_feature_scales(int3 pixel, float ccl_readonly_ptr buffer, float *scales, float ccl_readonly_ptr mean, int pass_stride)
{
	*(scales++) = fabsf(pixel.x - *(mean++)); //X
	*(scales++) = fabsf(pixel.y - *(mean++)); //Y
#ifdef DENOISE_TEMPORAL
	*(scales++) = fabsf(pixel.z - *(mean++)); //T
#endif

	*(scales++) = fabsf(ccl_get_feature(0) - *(mean++)); //Depth

	float normalS = len_squared(make_float3(ccl_get_feature(1) - mean[0], ccl_get_feature(2) - mean[1], ccl_get_feature(3) - mean[2]));
	mean += 3;
	*(scales++) = normalS; //NormalX
	*(scales++) = normalS; //NormalY
	*(scales++) = normalS; //NormalZ

	*(scales++) = fabsf(ccl_get_feature(4) - *(mean++)); //Shadow

	float normalT = len_squared(make_float3(ccl_get_feature(5) - mean[0], ccl_get_feature(6) - mean[1], ccl_get_feature(7) - mean[2]));
	mean += 3;
	*(scales++) = normalT; //AlbedoR
	*(scales++) = normalT; //AlbedoG
	*(scales++) = normalT; //AlbedoB
}

ccl_device_inline void filter_calculate_scale(float *scale)
{
	scale[0] = 1.0f/max(scale[0], 0.01f); //X
	scale[1] = 1.0f/max(scale[1], 0.01f); //Y
	scale += 2;
#ifdef DENOISE_TEMPORAL
	scale[0] = 1.0f/max(scale[0], 0.01f); //T
	scale++;
#endif
	
	scale[0] = 1.0f/max(scale[0], 0.01f); //Depth

	scale[1] = 1.0f/max(sqrtf(scale[1]), 0.01f); //NormalX
	scale[2] = 1.0f/max(sqrtf(scale[2]), 0.01f); //NormalY
	scale[3] = 1.0f/max(sqrtf(scale[3]), 0.01f); //NormalZ
	
	scale[4] = 1.0f/max(scale[4], 0.01f); //Shadow

	scale[5] = 1.0f/max(sqrtf(scale[5]), 0.01f); //AlbedoR
	scale[6] = 1.0f/max(sqrtf(scale[6]), 0.01f); //AlbedoG
	scale[7] = 1.0f/max(sqrtf(scale[7]), 0.01f); //AlbedoB
}

ccl_device_inline float3 filter_get_pixel_color(float ccl_readonly_ptr buffer, int channel, int pass_stride)
{
	return make_float3(ccl_get_feature(channel), ccl_get_feature(channel+1), ccl_get_feature(channel+2));
}

ccl_device_inline float filter_get_pixel_variance(float ccl_readonly_ptr buffer, int channel, int pass_stride)
{
	return average(make_float3(ccl_get_feature(channel), ccl_get_feature(channel+1), ccl_get_feature(channel+2)));
}

ccl_device_inline bool filter_firefly_rejection(float3 pixel_color, float pixel_variance, float3 center_color, float sqrt_center_variance)
{
	float color_diff = average(fabs(pixel_color - center_color));
	float variance = sqrt_center_variance + sqrtf(pixel_variance) + 0.005f;
	return (color_diff > 3.0f*variance);
}

/* Fill the design row without computing the weight. */
ccl_device_inline void filter_get_design_row_transform(int3 pixel, float ccl_readonly_ptr buffer, float ccl_readonly_ptr feature_means, int pass_stride, float *features, int rank, float *design_row, float ccl_readonly_ptr feature_transform, int transform_stride)
{
	filter_get_features(pixel, buffer, features, feature_means, pass_stride);
	design_row[0] = 1.0f;
	for(int d = 0; d < rank; d++) {
#ifdef __KERNEL_CUDA__
		float x = math_vector_dot_strided(features, feature_transform + d*DENOISE_FEATURES*transform_stride, transform_stride, DENOISE_FEATURES);
#else
		float x = math_vector_dot(features, feature_transform + d*DENOISE_FEATURES, DENOISE_FEATURES);
#endif
		design_row[1+d] = x;
	}
}

CCL_NAMESPACE_END