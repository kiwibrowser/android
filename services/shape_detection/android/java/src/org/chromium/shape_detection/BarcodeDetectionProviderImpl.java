// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;
import com.google.android.gms.vision.barcode.Barcode;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.mojo.bindings.InterfaceRequest;
import org.chromium.mojo.system.MojoException;
import org.chromium.services.service_manager.InterfaceFactory;
import org.chromium.shape_detection.mojom.BarcodeDetection;
import org.chromium.shape_detection.mojom.BarcodeDetectionProvider;
import org.chromium.shape_detection.mojom.BarcodeDetectorOptions;

/**
 * Service provider to create BarcodeDetection services
 */
public class BarcodeDetectionProviderImpl implements BarcodeDetectionProvider {
    private static final String TAG = "BarcodeProviderImpl";

    public BarcodeDetectionProviderImpl() {}

    @Override
    public void createBarcodeDetection(
            InterfaceRequest<BarcodeDetection> request, BarcodeDetectorOptions options) {
        BarcodeDetection.MANAGER.bind(new BarcodeDetectionImpl(options), request);
    }

    @Override
    public void enumerateSupportedFormats(EnumerateSupportedFormatsResponse callback) {
        int[] supportedFormats = {Barcode.AZTEC, Barcode.CODE_128, Barcode.CODE_39, Barcode.CODE_93,
                Barcode.CODABAR, Barcode.DATA_MATRIX, Barcode.EAN_13, Barcode.EAN_8, Barcode.ITF,
                Barcode.PDF417, Barcode.QR_CODE, Barcode.UPC_A, Barcode.UPC_E};
        callback.call(supportedFormats);
    }

    @Override
    public void close() {}

    @Override
    public void onConnectionError(MojoException e) {}

    /**
     * A factory class to register BarcodeDetectionProvider interface.
     */
    public static class Factory implements InterfaceFactory<BarcodeDetectionProvider> {
        public Factory() {}

        @Override
        public BarcodeDetectionProvider createImpl() {
            if (GoogleApiAvailability.getInstance().isGooglePlayServicesAvailable(
                        ContextUtils.getApplicationContext())
                    != ConnectionResult.SUCCESS) {
                Log.e(TAG, "Google Play Services not available");
                return null;
            }
            return new BarcodeDetectionProviderImpl();
        }
    }
}
