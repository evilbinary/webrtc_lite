/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/quic/reliablequicstream.h"

#include <string>

#include "webrtc/base/checks.h"

namespace cricket {

ReliableQuicStream::ReliableQuicStream(net::QuicStreamId id,
                                       net::QuicSession* session)
    : net::ReliableQuicStream(id, session) {
  RTC_DCHECK_NE(net::kCryptoStreamId, id);
}

ReliableQuicStream::~ReliableQuicStream() {}

void ReliableQuicStream::OnDataAvailable() {
  struct iovec iov;
  while (sequencer()->GetReadableRegions(&iov, 1) == 1) {
    SignalDataReceived(id(), reinterpret_cast<const char*>(iov.iov_base),
                       iov.iov_len);
    sequencer()->MarkConsumed(iov.iov_len);
  }
}

void ReliableQuicStream::OnClose() {
  net::ReliableQuicStream::OnClose();
  SignalClosed(id(), connection_error());
}

rtc::StreamResult ReliableQuicStream::Write(const char* data, size_t len) {
  // Writes the data, or buffers it.
  WriteOrBufferData(std::string(data, len), false, nullptr);
  if (HasBufferedData()) {
    return rtc::StreamResult(rtc::SR_BLOCK);
  }

  return rtc::StreamResult(rtc::SR_SUCCESS);
}

}  // namespace cricket
