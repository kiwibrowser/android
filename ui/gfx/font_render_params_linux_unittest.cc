// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_render_params.h"

#include <fontconfig/fontconfig.h>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/test/fontconfig_util_linux.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font.h"
#include "ui/gfx/linux_font_delegate.h"

namespace gfx {

namespace {

// Strings appearing at the beginning and end of Fontconfig XML files.
const char kFontconfigFileHeader[] =
    "<?xml version=\"1.0\"?>\n"
    "<!DOCTYPE fontconfig SYSTEM \"fonts.dtd\">\n"
    "<fontconfig>\n";
const char kFontconfigFileFooter[] = "</fontconfig>";

// Strings appearing at the beginning and end of Fontconfig <match> stanzas.
const char kFontconfigMatchFontHeader[] = "  <match target=\"font\">\n";
const char kFontconfigMatchPatternHeader[] = "  <match target=\"pattern\">\n";
const char kFontconfigMatchFooter[] = "  </match>\n";

// Implementation of LinuxFontDelegate that returns a canned FontRenderParams
// struct. This is used to isolate tests from the system's local configuration.
class TestFontDelegate : public LinuxFontDelegate {
 public:
  TestFontDelegate() {}
  ~TestFontDelegate() override {}

  void set_params(const FontRenderParams& params) { params_ = params; }

  FontRenderParams GetDefaultFontRenderParams() const override {
    return params_;
  }
  void GetDefaultFontDescription(std::string* family_out,
                                 int* size_pixels_out,
                                 int* style_out,
                                 Font::Weight* weight_out,
                                 FontRenderParams* params_out) const override {
    NOTIMPLEMENTED();
  }

 private:
  FontRenderParams params_;

  DISALLOW_COPY_AND_ASSIGN(TestFontDelegate);
};

// Instructs Fontconfig to load |path|, an XML configuration file, into the
// current config, returning true on success.
bool LoadConfigFileIntoFontconfig(const base::FilePath& path) {
  // Unlike other FcConfig functions, FcConfigParseAndLoad() doesn't default to
  // the current config when passed NULL. So that's cool.
  if (!FcConfigParseAndLoad(
          FcConfigGetCurrent(),
          reinterpret_cast<const FcChar8*>(path.value().c_str()), FcTrue)) {
    LOG(ERROR) << "Fontconfig failed to load " << path.value();
    return false;
  }
  return true;
}

// Writes |data| to a file in |temp_dir| and passes it to
// LoadConfigFileIntoFontconfig().
bool LoadConfigDataIntoFontconfig(const base::FilePath& temp_dir,
                                  const std::string& data) {
  base::FilePath path;
  if (!base::CreateTemporaryFileInDir(temp_dir, &path)) {
    PLOG(ERROR) << "Unable to create temporary file in " << temp_dir.value();
    return false;
  }
  if (base::WriteFile(path, data.data(), data.size()) !=
      static_cast<int>(data.size())) {
    PLOG(ERROR) << "Unable to write config data to " << path.value();
    return false;
  }
  return LoadConfigFileIntoFontconfig(path);
}

// Returns a Fontconfig <edit> stanza.
std::string CreateFontconfigEditStanza(const std::string& name,
                                       const std::string& type,
                                       const std::string& value) {
  return base::StringPrintf(
      "    <edit name=\"%s\" mode=\"assign\">\n"
      "      <%s>%s</%s>\n"
      "    </edit>\n",
      name.c_str(), type.c_str(), value.c_str(), type.c_str());
}

// Returns a Fontconfig <test> stanza.
std::string CreateFontconfigTestStanza(const std::string& name,
                                       const std::string& op,
                                       const std::string& type,
                                       const std::string& value) {
  return base::StringPrintf(
      "    <test name=\"%s\" compare=\"%s\" qual=\"any\">\n"
      "      <%s>%s</%s>\n"
      "    </test>\n",
      name.c_str(), op.c_str(), type.c_str(), value.c_str(), type.c_str());
}

// Returns a Fontconfig <alias> stanza.
std::string CreateFontconfigAliasStanza(const std::string& original_family,
                                        const std::string& preferred_family) {
  return base::StringPrintf(
      "  <alias>\n"
      "    <family>%s</family>\n"
      "    <prefer><family>%s</family></prefer>\n"
      "  </alias>\n",
      original_family.c_str(), preferred_family.c_str());
}

}  // namespace

class FontRenderParamsTest : public testing::Test {
 public:
  FontRenderParamsTest() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    original_font_delegate_ = LinuxFontDelegate::instance();
    LinuxFontDelegate::SetInstance(&test_font_delegate_);
    ClearFontRenderParamsCacheForTest();
  }

  ~FontRenderParamsTest() override {
    LinuxFontDelegate::SetInstance(
        const_cast<LinuxFontDelegate*>(original_font_delegate_));
  }

  void SetUp() override {
    // Fontconfig should already be set up by the test runner.
    DCHECK(FcConfigGetCurrent());
  }

  void TearDown() override {
    // We might have made a mess introducing new fontconfig settings.  Reset the
    // state of fontconfig for the next test.
    base::SetUpFontconfig();
  }

 protected:
  base::ScopedTempDir temp_dir_;
  const LinuxFontDelegate* original_font_delegate_;
  TestFontDelegate test_font_delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FontRenderParamsTest);
};

TEST_F(FontRenderParamsTest, Default) {
  ASSERT_TRUE(LoadConfigDataIntoFontconfig(
      temp_dir_.GetPath(),
      std::string(kFontconfigFileHeader) +
          // Specify the desired defaults via a font match rather than a pattern
          // match (since this is the style generally used in
          // /etc/fonts/conf.d).
          kFontconfigMatchFontHeader +
          CreateFontconfigEditStanza("antialias", "bool", "true") +
          CreateFontconfigEditStanza("autohint", "bool", "true") +
          CreateFontconfigEditStanza("hinting", "bool", "true") +
          CreateFontconfigEditStanza("hintstyle", "const", "hintslight") +
          CreateFontconfigEditStanza("rgba", "const", "rgb") +
          kFontconfigMatchFooter +
          // Add a font match for Arimo. Since it specifies a family, it
          // shouldn't take effect when querying default settings.
          kFontconfigMatchFontHeader +
          CreateFontconfigTestStanza("family", "eq", "string", "Arimo") +
          CreateFontconfigEditStanza("antialias", "bool", "true") +
          CreateFontconfigEditStanza("autohint", "bool", "false") +
          CreateFontconfigEditStanza("hinting", "bool", "true") +
          CreateFontconfigEditStanza("hintstyle", "const", "hintfull") +
          CreateFontconfigEditStanza("rgba", "const", "none") +
          kFontconfigMatchFooter +
          // Add font matches for fonts between 10 and 20 points or pixels.
          // Since they specify sizes, they also should not affect the defaults.
          kFontconfigMatchFontHeader +
          CreateFontconfigTestStanza("size", "more_eq", "double", "10.0") +
          CreateFontconfigTestStanza("size", "less_eq", "double", "20.0") +
          CreateFontconfigEditStanza("antialias", "bool", "false") +
          kFontconfigMatchFooter + kFontconfigMatchFontHeader +
          CreateFontconfigTestStanza("pixel_size", "more_eq", "double",
                                     "10.0") +
          CreateFontconfigTestStanza("pixel_size", "less_eq", "double",
                                     "20.0") +
          CreateFontconfigEditStanza("antialias", "bool", "false") +
          kFontconfigMatchFooter + kFontconfigFileFooter));

  FontRenderParams params = GetFontRenderParams(
      FontRenderParamsQuery(), NULL);
  EXPECT_TRUE(params.antialiasing);
  EXPECT_TRUE(params.autohinter);
  EXPECT_TRUE(params.use_bitmaps);
  EXPECT_EQ(FontRenderParams::HINTING_SLIGHT, params.hinting);
  EXPECT_FALSE(params.subpixel_positioning);
  EXPECT_EQ(FontRenderParams::SUBPIXEL_RENDERING_RGB,
            params.subpixel_rendering);
}

TEST_F(FontRenderParamsTest, Size) {
  ASSERT_TRUE(LoadConfigDataIntoFontconfig(
      temp_dir_.GetPath(),
      std::string(kFontconfigFileHeader) + kFontconfigMatchPatternHeader +
          CreateFontconfigEditStanza("antialias", "bool", "true") +
          CreateFontconfigEditStanza("hinting", "bool", "true") +
          CreateFontconfigEditStanza("hintstyle", "const", "hintfull") +
          CreateFontconfigEditStanza("rgba", "const", "none") +
          kFontconfigMatchFooter + kFontconfigMatchPatternHeader +
          CreateFontconfigTestStanza("pixelsize", "less_eq", "double", "10") +
          CreateFontconfigEditStanza("antialias", "bool", "false") +
          kFontconfigMatchFooter + kFontconfigMatchPatternHeader +
          CreateFontconfigTestStanza("size", "more_eq", "double", "20") +
          CreateFontconfigEditStanza("hintstyle", "const", "hintslight") +
          CreateFontconfigEditStanza("rgba", "const", "rgb") +
          kFontconfigMatchFooter + kFontconfigFileFooter));

  // The defaults should be used when the supplied size isn't matched by the
  // second or third blocks.
  FontRenderParamsQuery query;
  query.pixel_size = 12;
  FontRenderParams params = GetFontRenderParams(query, NULL);
  EXPECT_TRUE(params.antialiasing);
  EXPECT_EQ(FontRenderParams::HINTING_FULL, params.hinting);
  EXPECT_EQ(FontRenderParams::SUBPIXEL_RENDERING_NONE,
            params.subpixel_rendering);

  query.pixel_size = 10;
  params = GetFontRenderParams(query, NULL);
  EXPECT_FALSE(params.antialiasing);
  EXPECT_EQ(FontRenderParams::HINTING_FULL, params.hinting);
  EXPECT_EQ(FontRenderParams::SUBPIXEL_RENDERING_NONE,
            params.subpixel_rendering);

  query.pixel_size = 0;
  query.point_size = 20;
  params = GetFontRenderParams(query, NULL);
  EXPECT_TRUE(params.antialiasing);
  EXPECT_EQ(FontRenderParams::HINTING_SLIGHT, params.hinting);
  EXPECT_EQ(FontRenderParams::SUBPIXEL_RENDERING_RGB,
            params.subpixel_rendering);
}

TEST_F(FontRenderParamsTest, Style) {
  // Load a config that disables subpixel rendering for bold text and disables
  // hinting for italic text.
  ASSERT_TRUE(LoadConfigDataIntoFontconfig(
      temp_dir_.GetPath(),
      std::string(kFontconfigFileHeader) + kFontconfigMatchPatternHeader +
          CreateFontconfigEditStanza("antialias", "bool", "true") +
          CreateFontconfigEditStanza("hinting", "bool", "true") +
          CreateFontconfigEditStanza("hintstyle", "const", "hintslight") +
          CreateFontconfigEditStanza("rgba", "const", "rgb") +
          kFontconfigMatchFooter + kFontconfigMatchPatternHeader +
          CreateFontconfigTestStanza("weight", "eq", "const", "bold") +
          CreateFontconfigEditStanza("rgba", "const", "none") +
          kFontconfigMatchFooter + kFontconfigMatchPatternHeader +
          CreateFontconfigTestStanza("slant", "eq", "const", "italic") +
          CreateFontconfigEditStanza("hinting", "bool", "false") +
          kFontconfigMatchFooter + kFontconfigFileFooter));

  FontRenderParamsQuery query;
  query.style = Font::NORMAL;
  FontRenderParams params = GetFontRenderParams(query, NULL);
  EXPECT_EQ(FontRenderParams::HINTING_SLIGHT, params.hinting);
  EXPECT_EQ(FontRenderParams::SUBPIXEL_RENDERING_RGB,
            params.subpixel_rendering);

  query.weight = Font::Weight::BOLD;
  params = GetFontRenderParams(query, NULL);
  EXPECT_EQ(FontRenderParams::HINTING_SLIGHT, params.hinting);
  EXPECT_EQ(FontRenderParams::SUBPIXEL_RENDERING_NONE,
            params.subpixel_rendering);

  query.weight = Font::Weight::NORMAL;
  query.style = Font::ITALIC;
  params = GetFontRenderParams(query, NULL);
  EXPECT_EQ(FontRenderParams::HINTING_NONE, params.hinting);
  EXPECT_EQ(FontRenderParams::SUBPIXEL_RENDERING_RGB,
            params.subpixel_rendering);

  query.weight = Font::Weight::BOLD;
  query.style = Font::ITALIC;
  params = GetFontRenderParams(query, NULL);
  EXPECT_EQ(FontRenderParams::HINTING_NONE, params.hinting);
  EXPECT_EQ(FontRenderParams::SUBPIXEL_RENDERING_NONE,
            params.subpixel_rendering);
}

TEST_F(FontRenderParamsTest, Scalable) {
  // Load a config that only enables antialiasing for scalable fonts.
  ASSERT_TRUE(LoadConfigDataIntoFontconfig(
      temp_dir_.GetPath(),
      std::string(kFontconfigFileHeader) + kFontconfigMatchPatternHeader +
          CreateFontconfigEditStanza("antialias", "bool", "false") +
          kFontconfigMatchFooter + kFontconfigMatchPatternHeader +
          CreateFontconfigTestStanza("scalable", "eq", "bool", "true") +
          CreateFontconfigEditStanza("antialias", "bool", "true") +
          kFontconfigMatchFooter + kFontconfigFileFooter));

  // Check that we specifically ask how scalable fonts should be rendered.
  FontRenderParams params = GetFontRenderParams(
      FontRenderParamsQuery(), NULL);
  EXPECT_TRUE(params.antialiasing);
}

TEST_F(FontRenderParamsTest, UseBitmaps) {
  // Load a config that enables embedded bitmaps for fonts <= 10 pixels.
  ASSERT_TRUE(LoadConfigDataIntoFontconfig(
      temp_dir_.GetPath(),
      std::string(kFontconfigFileHeader) + kFontconfigMatchPatternHeader +
          CreateFontconfigEditStanza("embeddedbitmap", "bool", "false") +
          kFontconfigMatchFooter + kFontconfigMatchPatternHeader +
          CreateFontconfigTestStanza("pixelsize", "less_eq", "double", "10") +
          CreateFontconfigEditStanza("embeddedbitmap", "bool", "true") +
          kFontconfigMatchFooter + kFontconfigFileFooter));

  FontRenderParamsQuery query;
  FontRenderParams params = GetFontRenderParams(query, NULL);
  EXPECT_FALSE(params.use_bitmaps);

  query.pixel_size = 5;
  params = GetFontRenderParams(query, NULL);
  EXPECT_TRUE(params.use_bitmaps);
}

TEST_F(FontRenderParamsTest, ForceFullHintingWhenAntialiasingIsDisabled) {
  // Load a config that disables antialiasing and hinting while requesting
  // subpixel rendering.
  ASSERT_TRUE(LoadConfigDataIntoFontconfig(
      temp_dir_.GetPath(),
      std::string(kFontconfigFileHeader) + kFontconfigMatchPatternHeader +
          CreateFontconfigEditStanza("antialias", "bool", "false") +
          CreateFontconfigEditStanza("hinting", "bool", "false") +
          CreateFontconfigEditStanza("hintstyle", "const", "hintnone") +
          CreateFontconfigEditStanza("rgba", "const", "rgb") +
          kFontconfigMatchFooter + kFontconfigFileFooter));

  // Full hinting should be forced. See the comment in GetFontRenderParams() for
  // more information.
  FontRenderParams params = GetFontRenderParams(
      FontRenderParamsQuery(), NULL);
  EXPECT_FALSE(params.antialiasing);
  EXPECT_EQ(FontRenderParams::HINTING_FULL, params.hinting);
  EXPECT_EQ(FontRenderParams::SUBPIXEL_RENDERING_NONE,
            params.subpixel_rendering);
  EXPECT_FALSE(params.subpixel_positioning);
}

TEST_F(FontRenderParamsTest, ForceSubpixelPositioning) {
  {
    FontRenderParams params =
        GetFontRenderParams(FontRenderParamsQuery(), NULL);
    EXPECT_TRUE(params.antialiasing);
    EXPECT_FALSE(params.subpixel_positioning);
    SetFontRenderParamsDeviceScaleFactor(1.0f);
  }
  ClearFontRenderParamsCacheForTest();
  SetFontRenderParamsDeviceScaleFactor(1.25f);
  // Subpixel positioning should be forced.
  {
    FontRenderParams params =
        GetFontRenderParams(FontRenderParamsQuery(), NULL);
    EXPECT_TRUE(params.antialiasing);
    EXPECT_TRUE(params.subpixel_positioning);
    SetFontRenderParamsDeviceScaleFactor(1.0f);
  }
  ClearFontRenderParamsCacheForTest();
  SetFontRenderParamsDeviceScaleFactor(2.f);
  // Subpixel positioning should be forced on non-Chrome-OS.
  {
    FontRenderParams params =
        GetFontRenderParams(FontRenderParamsQuery(), nullptr);
    EXPECT_TRUE(params.antialiasing);
#if !defined(OS_CHROMEOS)
    EXPECT_TRUE(params.subpixel_positioning);
#else
    // Integral scale factor does not require subpixel positioning.
    EXPECT_FALSE(params.subpixel_positioning);
#endif  // !defined(OS_CHROMEOS)
    SetFontRenderParamsDeviceScaleFactor(1.0f);
  }
}

TEST_F(FontRenderParamsTest, OnlySetConfiguredValues) {
  // Configure the LinuxFontDelegate (which queries GtkSettings on desktop
  // Linux) to request subpixel rendering.
  FontRenderParams system_params;
  system_params.subpixel_rendering = FontRenderParams::SUBPIXEL_RENDERING_RGB;
  test_font_delegate_.set_params(system_params);

  // Load a Fontconfig config that enables antialiasing but doesn't say anything
  // about subpixel rendering.
  ASSERT_TRUE(LoadConfigDataIntoFontconfig(
      temp_dir_.GetPath(),
      std::string(kFontconfigFileHeader) + kFontconfigMatchPatternHeader +
          CreateFontconfigEditStanza("antialias", "bool", "true") +
          kFontconfigMatchFooter + kFontconfigFileFooter));

  // The subpixel rendering setting from the delegate should make it through.
  FontRenderParams params = GetFontRenderParams(
      FontRenderParamsQuery(), NULL);
  EXPECT_EQ(system_params.subpixel_rendering, params.subpixel_rendering);
}

TEST_F(FontRenderParamsTest, NoFontconfigMatch) {
  // A default configuration was set up globally.  Reset it to a blank config.
  FcConfigSetCurrent(FcConfigCreate());

  FontRenderParams system_params;
  system_params.antialiasing = true;
  system_params.hinting = FontRenderParams::HINTING_MEDIUM;
  system_params.subpixel_rendering = FontRenderParams::SUBPIXEL_RENDERING_RGB;
  test_font_delegate_.set_params(system_params);

  FontRenderParamsQuery query;
  query.families.push_back("Arimo");
  query.families.push_back("Times New Roman");
  query.pixel_size = 10;
  std::string suggested_family;
  FontRenderParams params = GetFontRenderParams(query, &suggested_family);

  // The system params and the first requested family should be returned.
  EXPECT_EQ(system_params.antialiasing, params.antialiasing);
  EXPECT_EQ(system_params.hinting, params.hinting);
  EXPECT_EQ(system_params.subpixel_rendering, params.subpixel_rendering);
  EXPECT_EQ(query.families[0], suggested_family);
}

TEST_F(FontRenderParamsTest, MissingFamily) {
  // With Arimo and Verdana installed, request (in order) Helvetica, Arimo, and
  // Verdana and check that Arimo is returned.
  FontRenderParamsQuery query;
  query.families.push_back("Helvetica");
  query.families.push_back("Arimo");
  query.families.push_back("Verdana");
  std::string suggested_family;
  GetFontRenderParams(query, &suggested_family);
  EXPECT_EQ("Arimo", suggested_family);
}

TEST_F(FontRenderParamsTest, SubstituteFamily) {
  // Configure Fontconfig to use Tinos for both Helvetica and Arimo.
  ASSERT_TRUE(LoadConfigDataIntoFontconfig(
      temp_dir_.GetPath(),
      std::string(kFontconfigFileHeader) +
          CreateFontconfigAliasStanza("Helvetica", "Tinos") +
          kFontconfigMatchPatternHeader +
          CreateFontconfigTestStanza("family", "eq", "string", "Arimo") +
          CreateFontconfigEditStanza("family", "string", "Tinos") +
          kFontconfigMatchFooter + kFontconfigFileFooter));

  FontRenderParamsQuery query;
  query.families.push_back("Helvetica");
  std::string suggested_family;
  GetFontRenderParams(query, &suggested_family);
  EXPECT_EQ("Tinos", suggested_family);

  query.families.clear();
  query.families.push_back("Arimo");
  suggested_family.clear();
  GetFontRenderParams(query, &suggested_family);
  EXPECT_EQ("Tinos", suggested_family);
}

}  // namespace gfx
