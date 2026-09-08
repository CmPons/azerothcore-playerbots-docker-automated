{
  description = "Pinned Pi runtime for the AzerothCore chatter bridge (no NixOS changes)";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/c043004d1c6985732bcc1cbc5a9c9aecbbb4e0f0";

  outputs = { nixpkgs, ... }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
    in
    {
      packages = nixpkgs.lib.genAttrs systems (system:
        let
          pkgs = import nixpkgs { inherit system; };
        in
        {
          pi = pkgs.pi-coding-agent;
          default = pkgs.pi-coding-agent;
        });
    };
}
