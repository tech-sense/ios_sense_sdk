Pod::Spec.new do |s|
  s.name         = 'SenseSDK'
  s.version      = '0.0.4'
  s.summary      = 'Sense iOS SDK'
  s.description  = <<-DESC
                  Sense is a device intelligence and identification tool. This tool collects a comprehensive set of attributes unique to a device or browser, forming an identity that will help businesses.
                   DESC
  s.homepage     = 'https://pro.getsense.co'
  s.license      = { :type => "Apache License, Version 2.0", :text => <<-LICENSE
      Licensed under the Apache License, Version 2.0 (the "License");
      you may not use this file except in compliance with the License.
      You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

      Unless required by applicable law or agreed to in writing, software
      distributed under the License is distributed on an "AS IS" BASIS,
      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
      See the License for the specific language governing permissions and
      limitations under the License.
      LICENSE
  }
  s.author       = { "Sense" => "tech@getsense.co" }
  s.source       = { :git => "https://github.com/tech-sense/ios_sense_sdk.git", :tag => s.version.to_s }
  s.vendored_frameworks = 'XCFramework/SenseSDK.xcframework'
  s.platform     = :ios, '12.0'
end
