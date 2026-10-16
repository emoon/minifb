plugins {
    alias(libs.plugins.android.application)
}

android {
    namespace = "com.example.noise"
    compileSdk {
        version = release(36)
    }

    defaultConfig {
        applicationId = "com.example.noise26"
        minSdk = 24
        targetSdk = 36
        versionCode = 1
        versionName = "1.0"

        externalNativeBuild {
            cmake {
                // ./gradlew assembleDebug -PminifbUseMmgr=ON
                arguments += "-DMINIFB_USE_MMGR=${project.findProperty("minifbUseMmgr") ?: "OFF"}"
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}
