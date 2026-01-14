neoguvc
===================

Teste espelhamento

> Este repositório deriva do projeto original **guvcview** (https://guvcview.sourceforge.net/). Licença e créditos completos encontram-se em `COPYING`, `AUTHORS` e demais arquivos herdados.

Guia mínimo para compilar, executar em modo desenvolvimento e instalar a
aplicação.

Compilar
--------
```bash
cmake -S . -B build -DUSE_SFML=ON
cmake --build build
```

Rodar como desenvolvedor
------------------------
```bash
./run.sh [opções]
```
O script prepara `LD_LIBRARY_PATH` para usar as bibliotecas geradas em `build/`
e executa `build/guvcview/guvcview`. Passe qualquer parâmetro adicional na
mesma linha.

Instalar no sistema
-------------------
```bash
sudo cmake --install build
```
Use `DESTDIR=` se precisar gerar um diretório raiz alternativo (por exemplo,
para empacotar manualmente).

Empacotar (.deb nativo)
------------------------
```bash
debian/rules clean      # remove artefatos anteriores
debuild -uc -us         # gera os pacotes .deb na pasta pai (..)
```
Certifique-se de ter as ferramentas do `devscripts` instaladas (fornece
`debuild`).

Estrutura do projeto
--------------------
- data/ - arquivos de dados instaláveis (desktop file, manpage, ícones, etc.)
- debian/ - metadados e scripts para empacotamento no formato .deb
- guvcview/ - código legado da interface GTK original
- gview_audio/ - biblioteca C responsável pela captura e manipulação de áudio
- gview_encoder/ - biblioteca C que encapsula o pipeline de encoders/libav
- gview_render/ - utilitários de renderização e overlays herdados do projeto original
- gview_v4l2core/ - camada de acesso direto ao dispositivo V4L2 usada pelo app
- includes/ - cabeçalhos compartilhados entre os módulos C herdados
- po/ - arquivos de tradução (gettext)
- ui/ - implementação da nova interface (GTKmm) e integrações com as libs C



