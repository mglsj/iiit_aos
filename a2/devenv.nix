{
  pkgs,
  ...
}:
{
  packages = with pkgs; [
    cmake
    readline
  ];

  stdenv = pkgs.gcc16Stdenv;
}
