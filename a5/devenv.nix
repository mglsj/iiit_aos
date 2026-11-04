{
  pkgs,
  ...
}:

{
  packages = [
    pkgs.uv
    pkgs.jdk11
  ];

  languages.python = {
    enable = true;
    version = "3.10";
    uv = {
      enable = true;
      sync.enable = true;
    };
  };

  env.JAVA_HOME = "${pkgs.jdk11.home}";
  env.PYSPARK_PYTHON = "python3";
}
