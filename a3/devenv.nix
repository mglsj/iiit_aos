{
  pkgs,
  ...
}:
{
  packages = with pkgs; [
    cmake
  ];

  stdenv = pkgs.gcc16Stdenv;
}
