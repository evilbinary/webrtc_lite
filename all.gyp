# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'variables': {
    'include_examples%': 1,
    'include_tests%': 0,
    'webrtc_root_additional_dependencies': [],
  },
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        'webrtc/webrtc.gyp:*',
        '<@(webrtc_root_additional_dependencies)',
      ],
      'conditions': [
        ['include_examples==1', {
          'dependencies': [
            'webrtc/webrtc_examples.gyp:*',
          ],
        }],
        ['OS=="ios" or (OS=="mac" and target_arch!="ia32")', {
          'dependencies': [
            'talk/app/webrtc/legacy_objc_api.gyp:*',
          ],
          'conditions': [
            ['include_tests==1', {
              'dependencies': [
                'talk/app/webrtc/legacy_objc_api_tests.gyp:*',
              ],
            }],
          ],
        }],
      ],
    },
{
			'target_name': 'libwebrtc',
			'type': 'shared_library',
			'dependencies': [
			  '<(webrtc_root)/api/api.gyp:libjingle_peerconnection',
				'<(webrtc_root)/system_wrappers/system_wrappers.gyp:field_trial_default',
				'<@(webrtc_root_additional_dependencies)',
			],
			'conditions': [
				['OS=="linux"', {
					'cflags': [
						'<!@(pkg-config --cflags glib-2.0 gobject-2.0 gtk+-2.0)',
					],
					'link_settings': {
						'ldflags': [
						  '<!@(pkg-config --libs-only-L --libs-only-other glib-2.0'
							  ' gobject-2.0 gthread-2.0 gtk+-2.0)',
						],
						'libraries': [
						  '<!@(pkg-config --libs-only-l glib-2.0 gobject-2.0'
							  ' gthread-2.0 gtk+-2.0)',
										'-lstdc++',
						],
					},
				}],#linux

				['OS=="android"', {
					'dependencies': [
						'webrtc/api/api.gyp:libjingle_peerconnection_jni',
					],
				}],#android
				
				['OS=="win"', {
					'msvs_settings': {
						'VCCLCompilerTool': {
							
						},
						'VCLinkerTool': {
						  'AdditionalDependencies': [
							'd3d9.lib',
							'gdi32.lib',
							'strmiids.lib',
							'winmm.lib',
						  ],
						  'AdditionalOptions': [
							  '/NOENTRY',
						  ],
						},
						
					},
				}],#win

			],#conditions	
		},
  ],
}
