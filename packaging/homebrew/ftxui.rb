class Ftxui < Formula
  desc "CLI tool and TUI framework for building terminal applications"
  homepage "https://github.com/ArthurSonzogni/FTXUI"
  version "6.2.0"
  license "MIT"

  # Source tarball — update SHA256 when releasing
  url "https://github.com/ArthurSonzogni/FTXUI/archive/refs/tags/v#{version}.tar.gz"
  sha256 "0000000000000000000000000000000000000000000000000000000000000000" # placeholder

  head "https://github.com/ArthurSonzogni/FTXUI.git", branch: "main"

  depends_on "cmake" => :build
  depends_on :macos

  def install
    system "cmake", "-S", ".", "-B", "build",
           "-DCMAKE_BUILD_TYPE=Release",
           "-DFTXUI_BUILD_EXAMPLES=OFF",
           "-DFTXUI_BUILD_TESTS=OFF",
           "-DFTXUI_ENABLE_INSTALL=ON",
           *std_cmake_args
    system "cmake", "--build", "build", "--parallel"
    system "cmake", "--install", "build"

    # Install the CLI scaffolding tool
    bin.install "tools/ftxui"
  end

  test do
    assert_match "ftxui 6.2.0", shell_output("#{bin}/ftxui version")
  end
end
