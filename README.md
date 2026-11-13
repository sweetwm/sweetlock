<h1 align=center>sweetlock</h1>

<div align=center>

![GitHub last commit](https://img.shields.io/github/last-commit/sweetwm/sweetlock?style=for-the-badge&labelColor=181825&color=a6e3a1)
![GitHub repo size](https://img.shields.io/github/repo-size/sweetwm/sweetlock?style=for-the-badge&labelColor=181825&color=d3bfe6)
![AUR Version](https://img.shields.io/aur/version/sweetshot-bin?style=for-the-badge&labelColor=181825&color=b4befe)
![GitHub Repo stars](https://img.shields.io/github/stars/sweetwm/sweetlock?style=for-the-badge&labelColor=181825&color=f9e2af)

Lightweight screen locker for Wayland compositor

[< Wiki >](https://sweetwm.org/docs/sweetlock)

<a href="https://github.com/user-attachments/assets/332e05bc-bfed-447a-a67d-f37195b5a471" target="_blank">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://github.com/user-attachments/assets/332e05bc-bfed-447a-a67d-f37195b5a471" />
    <img alt="veila-preview2" src="https://github.com/user-attachments/assets/332e05bc-bfed-447a-a67d-f37195b5a471" />
  </picture>
</a>

</div>

## 🔥 Features

- Clock, date, avatar, username and a password field
- Backgrounds are cached, next lock is one file read
- Your wallpaper with blur and dim, or a color or gradient
- Works with fingerprint readers and security keys (experimental)
- Turns the screens off while locked


## Install

### Arch Linux

Install sweetlock from the AUR:

```bash
# prebuilt release package
yay -S sweetlock-bin

# git
yay -S sweetlock-git
```

---

### NixOS

Add the flake to your inputs and its overlay to nixpkgs:

```nix
{
  inputs = {
    nixpkgs.url = "nixpkgs/nixos-26.05";
    sweetlock = {
      url = "github:naurissteins/sweetlock";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = {nixpkgs, sweetlock, ...}: {
    nixosConfigurations.myhost = nixpkgs.lib.nixosSystem {
      system = "x86_64-linux";
      modules = [
        ./configuration.nix
        {nixpkgs.overlays = [sweetlock.overlays.default];}
      ];
    };
  };
}
```

Then install it and add its PAM service in `configuration.nix`:

```nix
environment.systemPackages = [pkgs.sweetlock];
security.pam.services.sweetlock = {};
```

> [!IMPORTANT]
> Without the PAM service every password is refused. After rebuilding, run
> `sweetlock --check` before your first lock.

---

### From source

Needs meson, wayland, wayland-protocols and zlib:

```bash
meson setup build --buildtype=release
meson compile -C build
sudo meson install -C build
```

---

## Usage

```bash
sweetlock           # lock until you unlock
sweetlock -f        # return once the screen is locked
sweetlock --check   # check the config, PAM, and images
```

[See all commands](https://sweetwm.org/docs/sweetlock/usage)

---

<p align=center>
  <img width="150" src="https://github.com/user-attachments/assets/8bf2ed35-777a-4160-a67c-38ea5df59b7f" alt ="Sweets" />
</p>
