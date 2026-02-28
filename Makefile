TARGET			= Trigon-Legacy
BUILD_CONFIG 	= Release

CLANG 			= clang -isysroot "$(shell xcrun --show-sdk-path --sdk iphoneos)"
ARCH 			= -arch arm64
FRAMEWORKS		= -framework IOKit -framework Foundation
C_FLAGS 		= -I./$(TARGET)/ -I./$(TARGET)/include
BINARY_FLAGS	= -mios-version-min=8.0

.PHONY: all clean ipa

all: clean ipa

ipa:
	xcodebuild clean build CODE_SIGN_IDENTITY="" CODE_SIGNING_REQUIRED=NO PRODUCT_BUNDLE_IDENTIFIER="com.therealclarity.$(TARGET)" -sdk iphoneos -configuration $(BUILD_CONFIG)
	rm -rf Payload
	mkdir Payload
	cp -r build/$(BUILD_CONFIG)-iphoneos/$(TARGET).app Payload/
	xattr -cr Payload/$(TARGET).app
	strip Payload/$(TARGET).app/$(TARGET)
	zip -r9 $(TARGET).ipa Payload

binary:
	$(CLANG) $(C_FLAGS) $(BINARY_FLAGS) $(ARCH) $(FRAMEWORKS) -o trigon-legacy.bin binary.c Trigon-Legacy/*.c
	strip trigon-legacy.bin
	ldid -Hsha1 -S trigon-legacy.bin

debug_binary:
	$(CLANG) $(C_FLAGS) -DDEBUG -g $(BINARY_FLAGS) $(ARCH) $(FRAMEWORKS) -o trigon-legacy-debug.bin binary.c Trigon-Legacy/*.c
	ldid -Hsha1 -S trigon-legacy-debug.bin

clean:
	rm -rf build Payload $(TARGET).ipa **/*.deb *.bin
