// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/tools/quic_client_base.h"

#include "net/third_party/quic/core/crypto/quic_random.h"
#include "net/third_party/quic/core/http/spdy_utils.h"
#include "net/third_party/quic/core/quic_server_id.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/core/tls_client_handshaker.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_text_utils.h"

namespace quic {

QuicClientBase::NetworkHelper::~NetworkHelper() = default;

QuicClientBase::QuicClientBase(
    const QuicServerId& server_id,
    const ParsedQuicVersionVector& supported_versions,
    const QuicConfig& config,
    QuicConnectionHelperInterface* helper,
    QuicAlarmFactory* alarm_factory,
    std::unique_ptr<NetworkHelper> network_helper,
    std::unique_ptr<ProofVerifier> proof_verifier)
    : server_id_(server_id),
      initialized_(false),
      local_port_(0),
      config_(config),
      crypto_config_(std::move(proof_verifier),
                     TlsClientHandshaker::CreateSslCtx()),
      helper_(helper),
      alarm_factory_(alarm_factory),
      supported_versions_(supported_versions),
      initial_max_packet_length_(0),
      num_stateless_rejects_received_(0),
      num_sent_client_hellos_(0),
      connection_error_(QUIC_NO_ERROR),
      connected_or_attempting_connect_(false),
      network_helper_(std::move(network_helper)) {}

QuicClientBase::~QuicClientBase() = default;

bool QuicClientBase::Initialize() {
  num_sent_client_hellos_ = 0;
  num_stateless_rejects_received_ = 0;
  connection_error_ = QUIC_NO_ERROR;
  connected_or_attempting_connect_ = false;

  // If an initial flow control window has not explicitly been set, then use the
  // same values that Chrome uses.
  const uint32_t kSessionMaxRecvWindowSize = 15 * 1024 * 1024;  // 15 MB
  const uint32_t kStreamMaxRecvWindowSize = 6 * 1024 * 1024;    //  6 MB
  if (config()->GetInitialStreamFlowControlWindowToSend() ==
      kMinimumFlowControlSendWindow) {
    config()->SetInitialStreamFlowControlWindowToSend(kStreamMaxRecvWindowSize);
  }
  if (config()->GetInitialSessionFlowControlWindowToSend() ==
      kMinimumFlowControlSendWindow) {
    config()->SetInitialSessionFlowControlWindowToSend(
        kSessionMaxRecvWindowSize);
  }

  if (!network_helper_->CreateUDPSocketAndBind(server_address_,
                                               bind_to_address_, local_port_)) {
    return false;
  }

  initialized_ = true;
  return true;
}

bool QuicClientBase::Connect() {
  // Attempt multiple connects until the maximum number of client hellos have
  // been sent.
  while (!connected() &&
         GetNumSentClientHellos() <= QuicCryptoClientStream::kMaxClientHellos) {
    StartConnect();
    while (EncryptionBeingEstablished()) {
      WaitForEvents();
    }
    if (GetQuicReloadableFlag(enable_quic_stateless_reject_support) &&
        connected()) {
      // Resend any previously queued data.
      ResendSavedData();
    }
    ParsedQuicVersion version = UnsupportedQuicVersion();
    if (session() != nullptr &&
        session()->error() != QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT &&
        !CanReconnectWithDifferentVersion(&version)) {
      // We've successfully created a session but we're not connected, and there
      // is no stateless reject to recover from and cannot try to reconnect with
      // different version.  Give up trying.
      break;
    }
  }
  if (!connected() &&
      GetNumSentClientHellos() > QuicCryptoClientStream::kMaxClientHellos &&
      session() != nullptr &&
      session()->error() == QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT) {
    // The overall connection failed due too many stateless rejects.
    set_connection_error(QUIC_CRYPTO_TOO_MANY_REJECTS);
  }
  return session()->connection()->connected();
}

void QuicClientBase::StartConnect() {
  DCHECK(initialized_);
  DCHECK(!connected());
  QuicPacketWriter* writer = network_helper_->CreateQuicPacketWriter();
  ParsedQuicVersion mutual_version = UnsupportedQuicVersion();
  const bool can_reconnect_with_different_version =
      CanReconnectWithDifferentVersion(&mutual_version);
  if (connected_or_attempting_connect()) {
    // If the last error was not a stateless reject, then the queued up data
    // does not need to be resent.
    // Keep queued up data if client can try to connect with a different
    // version.
    if (session()->error() != QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT &&
        !can_reconnect_with_different_version) {
      ClearDataToResend();
    }
    // Before we destroy the last session and create a new one, gather its stats
    // and update the stats for the overall connection.
    UpdateStats();
  }

  session_ = CreateQuicClientSession(
      supported_versions(),
      new QuicConnection(GetNextConnectionId(), server_address(), helper(),
                         alarm_factory(), writer,
                         /* owns_writer= */ false, Perspective::IS_CLIENT,
                         can_reconnect_with_different_version
                             ? ParsedQuicVersionVector{mutual_version}
                             : supported_versions()));
  if (initial_max_packet_length_ != 0) {
    session()->connection()->SetMaxPacketLength(initial_max_packet_length_);
  }
  // Reset |writer()| after |session()| so that the old writer outlives the old
  // session.
  set_writer(writer);
  InitializeSession();
  set_connected_or_attempting_connect(true);
}

void QuicClientBase::InitializeSession() {
  session()->Initialize();
}

void QuicClientBase::Disconnect() {
  DCHECK(initialized_);

  initialized_ = false;
  if (connected()) {
    session()->connection()->CloseConnection(
        QUIC_PEER_GOING_AWAY, "Client disconnecting",
        ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
  }

  ClearDataToResend();

  network_helper_->CleanUpAllUDPSockets();
}

ProofVerifier* QuicClientBase::proof_verifier() const {
  return crypto_config_.proof_verifier();
}

bool QuicClientBase::EncryptionBeingEstablished() {
  return !session_->IsEncryptionEstablished() &&
         session_->connection()->connected();
}

bool QuicClientBase::WaitForEvents() {
  DCHECK(connected());

  network_helper_->RunEventLoop();

  DCHECK(session() != nullptr);
  ParsedQuicVersion version = UnsupportedQuicVersion();
  if (!connected() &&
      (session()->error() == QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT ||
       CanReconnectWithDifferentVersion(&version))) {
    if (session()->error() == QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT) {
      DCHECK(GetQuicReloadableFlag(enable_quic_stateless_reject_support));
      QUIC_DLOG(INFO) << "Detected stateless reject while waiting for events.  "
                      << "Attempting to reconnect.";
    } else {
      QUIC_DLOG(INFO) << "Can reconnect with version: " << version
                      << ", attempting to reconnect.";
    }
    Connect();
  }

  return session()->num_active_requests() != 0;
}

bool QuicClientBase::MigrateSocket(const QuicIpAddress& new_host) {
  return MigrateSocketWithSpecifiedPort(new_host, local_port_);
}

bool QuicClientBase::MigrateSocketWithSpecifiedPort(
    const QuicIpAddress& new_host,
    int port) {
  if (!connected()) {
    return false;
  }

  network_helper_->CleanUpAllUDPSockets();

  set_bind_to_address(new_host);
  if (!network_helper_->CreateUDPSocketAndBind(server_address_,
                                               bind_to_address_, port)) {
    return false;
  }

  session()->connection()->SetSelfAddress(
      network_helper_->GetLatestClientAddress());

  QuicPacketWriter* writer = network_helper_->CreateQuicPacketWriter();
  set_writer(writer);
  session()->connection()->SetQuicPacketWriter(writer, false);

  return true;
}

QuicSession* QuicClientBase::session() {
  return session_.get();
}

QuicClientBase::NetworkHelper* QuicClientBase::network_helper() {
  return network_helper_.get();
}

const QuicClientBase::NetworkHelper* QuicClientBase::network_helper() const {
  return network_helper_.get();
}

void QuicClientBase::WaitForStreamToClose(QuicStreamId id) {
  DCHECK(connected());

  while (connected() && !session_->IsClosedStream(id)) {
    WaitForEvents();
  }
}

bool QuicClientBase::WaitForCryptoHandshakeConfirmed() {
  DCHECK(connected());

  while (connected() && !session_->IsCryptoHandshakeConfirmed()) {
    WaitForEvents();
  }

  // If the handshake fails due to a timeout, the connection will be closed.
  QUIC_LOG_IF(ERROR, !connected()) << "Handshake with server failed.";
  return connected();
}

bool QuicClientBase::connected() const {
  return session_.get() && session_->connection() &&
         session_->connection()->connected();
}

bool QuicClientBase::goaway_received() const {
  return session_ != nullptr && session_->goaway_received();
}

int QuicClientBase::GetNumSentClientHellos() {
  // If we are not actively attempting to connect, the session object
  // corresponds to the previous connection and should not be used.
  const int current_session_hellos = !connected_or_attempting_connect_
                                         ? 0
                                         : GetNumSentClientHellosFromSession();
  return num_sent_client_hellos_ + current_session_hellos;
}

void QuicClientBase::UpdateStats() {
  num_sent_client_hellos_ += GetNumSentClientHellosFromSession();
  if (session()->error() == QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT) {
    ++num_stateless_rejects_received_;
  }
}

int QuicClientBase::GetNumReceivedServerConfigUpdates() {
  // If we are not actively attempting to connect, the session object
  // corresponds to the previous connection and should not be used.
  // We do not need to take stateless rejects into account, since we
  // don't expect any scup messages to be sent during a
  // statelessly-rejected connection.
  return !connected_or_attempting_connect_
             ? 0
             : GetNumReceivedServerConfigUpdatesFromSession();
}

QuicErrorCode QuicClientBase::connection_error() const {
  // Return the high-level error if there was one.  Otherwise, return the
  // connection error from the last session.
  if (connection_error_ != QUIC_NO_ERROR) {
    return connection_error_;
  }
  if (session_ == nullptr) {
    return QUIC_NO_ERROR;
  }
  return session_->error();
}

QuicConnectionId QuicClientBase::GetNextConnectionId() {
  QuicConnectionId server_designated_id = GetNextServerDesignatedConnectionId();
  return !server_designated_id.IsEmpty() ? server_designated_id
                                         : GenerateNewConnectionId();
}

QuicConnectionId QuicClientBase::GetNextServerDesignatedConnectionId() {
  QuicCryptoClientConfig::CachedState* cached =
      crypto_config_.LookupOrCreate(server_id_);
  // If the cached state indicates that we should use a server-designated
  // connection ID, then return that connection ID.
  CHECK(cached != nullptr) << "QuicClientCryptoConfig::LookupOrCreate returned "
                           << "unexpected nullptr.";
  return cached->has_server_designated_connection_id()
             ? cached->GetNextServerDesignatedConnectionId()
             : EmptyQuicConnectionId();
}

QuicConnectionId QuicClientBase::GenerateNewConnectionId() {
  return QuicUtils::CreateRandomConnectionId(Perspective::IS_CLIENT);
}

bool QuicClientBase::CanReconnectWithDifferentVersion(
    ParsedQuicVersion* version) const {
  if (session_ == nullptr || session_->connection() == nullptr ||
      session_->error() != QUIC_INVALID_VERSION ||
      session_->connection()->server_supported_versions().empty()) {
    return false;
  }
  for (const auto& client_version : supported_versions_) {
    if (QuicContainsValue(session_->connection()->server_supported_versions(),
                          client_version)) {
      *version = client_version;
      return true;
    }
  }
  return false;
}

}  // namespace quic
