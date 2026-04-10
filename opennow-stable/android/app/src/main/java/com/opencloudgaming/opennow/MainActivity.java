package com.opencloudgaming.opennow;

import android.os.Bundle;

import com.getcapacitor.BridgeActivity;

public class MainActivity extends BridgeActivity {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        registerPlugin(LocalhostAuthPlugin.class);
        super.onCreate(savedInstanceState);
    }
}
