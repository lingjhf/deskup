#
# To learn more about a Podspec see http://guides.cocoapods.org/syntax/podspec.html.
# Run `pod lib lint deskup.podspec` to validate before publishing.
#
Pod::Spec.new do |s|
  s.name             = 'deskup'
  s.version          = '0.3.0'
  s.summary          = 'Desktop updates using Sparkle and Velopack.'
  s.description      = <<-DESC
Desktop updates using Sparkle and Velopack.
                       DESC
  s.homepage         = 'https://github.com/lingjhf/deskup'
  s.license          = { :type => 'MIT', :file => '../LICENSE' }
  s.author           = { 'lingjhf' => 'lingj.jhf@outlook.com' }

  s.source           = { :path => '.' }
  s.source_files = 'deskup/Sources/deskup/**/*'

  # If your plugin requires a privacy manifest, for example if it collects user
  # data, update the PrivacyInfo.xcprivacy file to describe your plugin's
  # privacy impact, and then uncomment this line. For more information,
  # see https://developer.apple.com/documentation/bundleresources/privacy_manifest_files
  # s.resource_bundles = {'deskup_privacy' => ['deskup/Sources/deskup/PrivacyInfo.xcprivacy']}

  s.dependency 'FlutterMacOS'
  s.dependency 'Sparkle', '2.10.0'

  s.platform = :osx, '12.0'
  s.pod_target_xcconfig = { 'DEFINES_MODULE' => 'YES' }
  s.swift_version = '5.0'
end
