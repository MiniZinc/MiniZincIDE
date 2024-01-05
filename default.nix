{ self, lib, stdenv, qt6, minizinc, makeWrapper, Cocoa }:

stdenv.mkDerivation rec {
  name = "minizinc-ide";

  src = self;
  nativeBuildInputs = [ qt6.qmake makeWrapper ];
  buildInputs = [ qt6.qtbase qt6.qtwebsockets ] ++ lib.optionals stdenv.isDarwin [ Cocoa ];
  dontWrapQtApps = true;

  executableLoc = if stdenv.isDarwin then "$out/bin/MiniZincIDE.app/Contents/MacOS/MiniZincIDE" else "$out/bin/MiniZincIDE";
  postInstall = ''
    wrapProgram ${executableLoc} --prefix PATH ":" ${lib.makeBinPath [ minizinc ]} --set QT_QPA_PLATFORM_PLUGIN_PATH "${lib.makeBinPath [ qt6.qtbase ]}/../lib/qt-6/plugins/platforms"
  '';

  meta = with lib; {
    homepage = "https://www.minizinc.org/";
    description = "IDE for MiniZinc, a medium-level constraint modelling language";
    longDescription = ''
      MiniZinc is a medium-level constraint modelling
      language. It is high-level enough to express most
      constraint problems easily, but low-level enough
      that it can be mapped onto existing solvers easily and consistently.
      It is a subset of the higher-level language Zinc.
    '';
    license = licenses.mpl20;
    platforms = platforms.unix;
  };
}
