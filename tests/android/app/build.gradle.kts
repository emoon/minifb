plugins {
    alias(libs.plugins.android.application)
}

android {
    namespace = "com.example.mouse_events_test"
    compileSdk {
        version = release(36)
    }

    defaultConfig {
        applicationId = "com.example.mouse_events_test"
        minSdk = 24
        targetSdk = 36
        versionCode = 1
        versionName = "1.0"
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}
