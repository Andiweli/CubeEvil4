plugins {
    id("com.android.application")
}

val supportedAbis = listOf("armeabi-v7a", "arm64-v8a")
val testAbi = providers.gradleProperty("re4.testAbi").orNull
require(testAbi == null || testAbi in supportedAbis) { "Unsupported re4.testAbi: $testAbi" }

android {
    namespace = "org.re4.port"
    compileSdk = 35
    buildToolsVersion = "35.0.0"
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "org.re4.port"
        minSdk = 21
        targetSdk = 35
        versionCode = 2
        versionName = "0.2.0"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        ndk {
            abiFilters += if (testAbi == null) supportedAbis else listOf(testAbi)
        }
        externalNativeBuild {
            cmake {
                arguments += listOf("-DRE4_BUILD_TESTS=OFF", "-DANDROID_STL=c++_static")
                targets += listOf("re4_port_jni")
            }
        }
    }
    externalNativeBuild {
        cmake {
            path = file("../../CMakeLists.txt")
            version = "4.1.2"
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }
}

dependencies {
    androidTestImplementation("androidx.test:runner:1.6.2")
    androidTestImplementation("junit:junit:4.13.2")
}
