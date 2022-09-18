pkgname=wg2nat-git
pkgver=r15.2e32645
pkgrel=1
pkgdesc="WireGuard through NAT"
arch=(x86_64 i686 armv7h aarch64)
depends=()
makedepends=(
    cmake
    git
)
provides=(wg2nat)
source=("${pkgname}::git+https://github.com/bsimku/wg2nat")
sha256sums=(SKIP)

pkgver() {
    cd "${pkgname}"
    printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
    cmake -B "$pkgname/build" -S "$pkgname" -DCMAKE_BUILD_TYPE=None -DCMAKE_INSTALL_PREFIX=/usr
    make -C "$pkgname/build"
}
package() {
    make -C "$pkgname/build" DESTDIR="$pkgdir" install
}
