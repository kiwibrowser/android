// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net_benchmarking.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net_benchmarking.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace {

void ClearPredictorCacheOnUIThread(
    base::WeakPtr<predictors::LoadingPredictor> loading_predictor,
    base::WeakPtr<chrome_browser_net::Predictor> predictor) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (loading_predictor)
    loading_predictor->resource_prefetch_predictor()->DeleteAllUrls();
  if (predictor)
    predictor->DiscardAllResultsAndClearPrefsOnUIThread();
}

}  // namespace

NetBenchmarking::NetBenchmarking(
    base::WeakPtr<predictors::LoadingPredictor> loading_predictor,
    base::WeakPtr<chrome_browser_net::Predictor> predictor,
    net::URLRequestContextGetter* request_context)
    : loading_predictor_(loading_predictor),
      predictor_(predictor),
      request_context_(request_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

NetBenchmarking::~NetBenchmarking() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

// static
void NetBenchmarking::Create(
    base::WeakPtr<predictors::LoadingPredictor> loading_predictor,
    base::WeakPtr<chrome_browser_net::Predictor> predictor,
    net::URLRequestContextGetter* request_context,
    chrome::mojom::NetBenchmarkingRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  mojo::MakeStrongBinding(
      std::make_unique<NetBenchmarking>(std::move(loading_predictor),
                                        std::move(predictor), request_context),
      std::move(request));
}

// static
bool NetBenchmarking::CheckBenchmarkingEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return command_line.HasSwitch(switches::kEnableNetBenchmarking);
}

void NetBenchmarking::ClearCache(const ClearCacheCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  int rv = -1;

  disk_cache::Backend* backend = request_context_->GetURLRequestContext()
                                     ->http_transaction_factory()
                                     ->GetCache()
                                     ->GetCurrentBackend();
  if (backend) {
    rv = backend->DoomAllEntries(callback);
    if (rv == net::ERR_IO_PENDING) {
      // Callback is handled by backend.
      return;
    }
  }
  callback.Run(rv);
}

void NetBenchmarking::ClearHostResolverCache(
    const ClearHostResolverCacheCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::HostCache* cache =
      request_context_->GetURLRequestContext()->host_resolver()->GetHostCache();
  if (cache)
    cache->clear();
  callback.Run();
}

void NetBenchmarking::CloseCurrentConnections(
    const CloseCurrentConnectionsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  request_context_->GetURLRequestContext()
      ->http_transaction_factory()
      ->GetCache()
      ->CloseAllConnections();
  callback.Run();
}

void NetBenchmarking::ClearPredictorCache(
    const ClearPredictorCacheCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTaskAndReply(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&ClearPredictorCacheOnUIThread, loading_predictor_,
                     predictor_),
      callback);
}
