cask "scancore" do
  version "1.0.0"
  sha256 "4c66eab2ec4820ea87b222c96856df2174751743acf31154ceb81933cbff871e"

  url "https://github.com/medium-gh/scancore/releases/download/v#{version}/SCANCORE-#{version}-macos-universal.dmg"
  name "SCANCORE"
  desc "Read comics and novels and watch anime in a native app"
  homepage "https://github.com/medium-gh/scancore"

  depends_on macos: :ventura

  app "SCANCORE.app"
end
