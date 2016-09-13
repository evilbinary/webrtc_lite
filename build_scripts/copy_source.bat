SET webrtc_root=G:\webrtc_win\src
   
xcopy \E \Y \Q %webrtc_root%\third_party\boringssl			  ..\third_party\boringssl
xcopy \E \Y \Q %webrtc_root%\third_party\expat               ..\third_party\expat
xcopy \E \Y \Q %webrtc_root%\third_party\gflags              ..\third_party\gflags
xcopy \E \Y \Q %webrtc_root%\third_party\jsoncpp             ..\third_party\jsoncpp
xcopy \E \Y \Q %webrtc_root%\third_party\libjpeg_turbo       ..\third_party\libjpeg_turbo
xcopy \E \Y \Q %webrtc_root%\third_party\libsrtp             ..\third_party\libsrtp
xcopy \E \Y \Q %webrtc_root%\third_party\libvpx              ..\third_party\libvpx
xcopy \E \Y \Q %webrtc_root%\third_party\libyuv              ..\third_party\libyuv
xcopy \E \Y \Q %webrtc_root%\third_party\ocmock              ..\third_party\ocmock
xcopy \E \Y \Q %webrtc_root%\third_party\openmax_dl          ..\third_party\openmax_dl
xcopy \E \Y \Q %webrtc_root%\third_party\opus                ..\third_party\opus
xcopy \E \Y \Q %webrtc_root%\third_party\protobuf            ..\third_party\protobuf
xcopy \E \Y \Q %webrtc_root%\third_party\usrsctp             ..\third_party\usrsctp
xcopy \E \Y \Q %webrtc_root%\third_party\yasm                ..\third_party\yasm


xcopy \E \Y \Q %webrtc_root%\third_party\winsdk_samples      ..\third_party\winsdk_samples


copy  %webrtc_root%\* ..\
xcopy \E \Y \Q %webrtc_root%\build ..\build
xcopy \E \Y \Q %webrtc_root%\webrtc ..\webrtc
xcopy \E \Y \Q %webrtc_root%\talk ..\talk
xcopy \E \Y \Q %webrtc_root%\buildtools ..\buildtools
xcopy \E \Y \Q %webrtc_root%\tools ..\tools
xcopy \E \Y \Q %webrtc_root%\testing ..\testing




