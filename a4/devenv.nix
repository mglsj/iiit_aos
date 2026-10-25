{
  pkgs,
  ...
}:

{
  packages = [
    pkgs.cmake
    pkgs.openssl
    pkgs.zlib
  ];

  stdenv = pkgs.gcc16Stdenv;
}
