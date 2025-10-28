neoguvc (fork GTK)
===================

> Este repositório deriva do projeto original **guvcview**  
> (https://guvcview.sourceforge.net/). Licença e créditos completos
> encontram-se em `COPYING`, `AUTHORS` e demais arquivos herdados.

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
