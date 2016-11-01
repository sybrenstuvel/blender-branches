"""
Python logging to Blender's info window
"""

import logging
import bpy


class BlenderInfoLogger(logging.Handler):
    def emit(self, record):
        print('Hey a record', record)
