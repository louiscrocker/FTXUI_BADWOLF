class FtxuiBadwolf < Formula
  desc "CLI tool and TUI framework for building terminal applications (FTXUI BadWolf)"
  homepage "https://github.com/louiscrocker/FTXUI_BADWOLF"
  version "6.2.0"
  license "MIT"

  # Source tarball — update SHA256 when releasing
  url "https://github.com/louiscrocker/FTXUI_BADWOLF/archive/refs/tags/v#{version}.tar.gz"
  sha256 "0000000000000000000000000000000000000000000000000000000000000000" # placeholder

  head "https://github.com/louiscrocker/FTXUI_BADWOLF.git", branch: "main"

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
    assert_match "badwolf 1.0.0", shell_output("#{bin}/ftxui version")
  end
end
