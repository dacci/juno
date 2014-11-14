// Copyright (c) 2014 dacci.org

#include "service/scissors/scissors_config.h"

#include <madoka/concurrent/lock_guard.h>

#include <string>

namespace {
const char kRemoteAddress[] = "RemoteAddress";
const char kRemotePort[] = "RemotePort";
const char kRemoteSSL[] = "RemoteSSL";
const char kRemoteUDP[] = "RemoteUDP";
}  // namespace

ScissorsConfig::ScissorsConfig()
    : remote_port_(), remote_ssl_(), remote_udp_() {
}

ScissorsConfig::ScissorsConfig(const ScissorsConfig& other)
    : ServiceConfig(other),
      remote_address_(other.remote_address_),
      remote_port_(other.remote_port_),
      remote_ssl_(other.remote_ssl_),
      remote_udp_(other.remote_udp_) {
}

ScissorsConfig::~ScissorsConfig() {
}

ScissorsConfig& ScissorsConfig::operator=(const ScissorsConfig& other) {
  if (&other != this) {
    madoka::concurrent::WriteLock write_lock(&lock_);
    madoka::concurrent::LockGuard guard(&write_lock);

    remote_address_ = other.remote_address_;
    remote_port_ = other.remote_port_;
    remote_ssl_ = other.remote_ssl_;
    remote_udp_ = other.remote_udp_;
  }

  return *this;
}

bool ScissorsConfig::Load(const RegistryKey& key) {
  madoka::concurrent::WriteLock write_lock(&lock_);
  madoka::concurrent::LockGuard guard(&write_lock);

  if (!key.QueryString(kRemoteAddress, &remote_address_))
    return false;

  if (!key.QueryInteger(kRemotePort, &remote_port_))
    return false;

  key.QueryInteger(kRemoteSSL, &remote_ssl_);
  key.QueryInteger(kRemoteUDP, &remote_udp_);

  return true;
}

bool ScissorsConfig::Save(RegistryKey* key) {
  madoka::concurrent::ReadLock read_lock(&lock_);
  madoka::concurrent::LockGuard guard(&read_lock);

  key->SetString(kRemoteAddress, remote_address_);
  key->SetInteger(kRemotePort, remote_port_);
  key->SetInteger(kRemoteSSL, remote_ssl_);
  key->SetInteger(kRemoteUDP, remote_udp_);

  return true;
}

std::string ScissorsConfig::remote_address() {
  madoka::concurrent::ReadLock read_lock(&lock_);
  madoka::concurrent::LockGuard guard(&read_lock);

  return remote_address_;
}

void ScissorsConfig::set_remote_address(const std::string& remote_address) {
  madoka::concurrent::WriteLock write_lock(&lock_);
  madoka::concurrent::LockGuard guard(&write_lock);

  remote_address_ = remote_address;
}

int ScissorsConfig::remote_port() {
  madoka::concurrent::ReadLock read_lock(&lock_);
  madoka::concurrent::LockGuard guard(&read_lock);

  return remote_port_;
}

void ScissorsConfig::set_remote_port(int remote_port) {
  madoka::concurrent::WriteLock write_lock(&lock_);
  madoka::concurrent::LockGuard guard(&write_lock);

  remote_port_ = remote_port;
}

int ScissorsConfig::remote_ssl() {
  madoka::concurrent::ReadLock read_lock(&lock_);
  madoka::concurrent::LockGuard guard(&read_lock);

  return remote_ssl_;
}

void ScissorsConfig::set_remote_ssl(int remote_ssl) {
  madoka::concurrent::WriteLock write_lock(&lock_);
  madoka::concurrent::LockGuard guard(&write_lock);

  remote_ssl_ = remote_ssl;
}

int ScissorsConfig::remote_udp() {
  madoka::concurrent::ReadLock read_lock(&lock_);
  madoka::concurrent::LockGuard guard(&read_lock);

  return remote_udp_;
}

void ScissorsConfig::set_remote_udp(int remote_udp) {
  madoka::concurrent::WriteLock write_lock(&lock_);
  madoka::concurrent::LockGuard guard(&write_lock);

  remote_udp_ = remote_udp;
}
