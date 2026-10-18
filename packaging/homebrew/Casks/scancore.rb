cask "scancore" do
  version "1.2.0"
  sha256 "00ccf7d89534f8a90d0a11c511a521a7a218554bf3c2ab3e16548b8c78af8537"

  url "https://github.com/medium-gh/scancore/releases/download/v#{version}/SCANCORE-#{version}-macos-universal.dmg"
  name "SCANCORE"
  desc "Read comics and novels and watch anime in a native app"
  homepage "https://github.com/medium-gh/scancore"

  depends_on macos: :ventura

  app "SCANCORE.app"
end
