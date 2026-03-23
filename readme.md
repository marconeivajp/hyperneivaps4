🎮 Hyper Neiva - Multitool (PS4 Homebrew)
Bem-vindo ao repositório do Hyper Neiva! Este é um aplicativo (homebrew) robusto e multifuncional desenvolvido em C++ para o sistema PlayStation 4. Ele atua como um verdadeiro canivete suíço para o seu console, oferecendo diversas ferramentas integradas em uma única interface gráfica intuitiva.

✨ Funcionalidades
O aplicativo conta com uma série de módulos integrados para expandir as capacidades do seu console:

🗂️ Explorador de Arquivos: Navegue pelos diretórios do sistema de forma fácil e rápida.

📝 Bloco de Notas / Editor: Crie, edite e visualize arquivos de texto diretamente pelo console.

☁️ Integração com Dropbox: Baixe arquivos diretamente da sua conta do Dropbox para o console de forma nativa.

🎵 Reprodutor de Áudio: Suporte para reprodução de arquivos de áudio nos formatos .mp3 e .wav.

🕹️ Gerenciador de Jogos / Menu: Interface customizável (com background, capas e ícones) para listar e gerenciar seus arquivos e jogos.

🌐 Network & Download: Capacidade de fazer requisições de rede e baixar arquivos de repositórios diretamente para o armazenamento local.

📦 Suporte a Arquivos Zipados: Extração e manipulação de arquivos usando a biblioteca miniz.

🚀 Como Usar e Instalar
Compilação: Se você deseja compilar do zero, o projeto possui suporte ao Visual Studio (via arquivo .vcxproj) e um script de compilação rápida (build.bat).

Instalação no PS4: Compile o projeto para gerar o arquivo .pkg (ou utilize o arquivo Hyper Neiva.pkg já gerado). Copie o .pkg para um pendrive formatado em exFAT e instale-o no seu PS4 (necessário ter o console desbloqueado) através do Package Installer.

Execução: Após a instalação, basta iniciar o aplicativo diretamente pelo menu principal (LiveArea) do PS4.

☁️ Configurando o Dropbox (IMPORTANTE)
Para que a funcionalidade de baixar arquivos diretamente do Dropbox funcione, você precisa configurar o seu próprio Token de Acesso (Access Token). Por questões de segurança, você não deve compartilhar seu token público.

O token deve ser inserido no arquivo localizado em:
📂 assets/dropbox_token.txt

🔑 Como obter o Token do Dropbox:
Acesse o Dropbox App Console.

Faça login com sua conta do Dropbox.

Clique no botão "Create app".

Escolha a opção "Scoped access".

Em Choose the type of access you need, selecione "App folder" (para acesso apenas a uma pasta específica) ou "Full Dropbox" (para acesso a todo o Dropbox), dependendo da sua necessidade.

Dê um nome ao seu aplicativo e clique em "Create app".

Na página de configurações do seu novo aplicativo, vá até a aba "Permissions" e certifique-se de marcar as permissões de leitura de arquivos (files.content.read / files.metadata.read) e salve.

Volte para a aba "Settings", role a página para baixo até encontrar a seção "OAuth 2".

Procure pela opção "Generated access token" e clique no botão "Generate".

Copie a longa sequência de letras e números que aparecerá.

Abra o arquivo assets/dropbox_token.txt no seu repositório, cole o código gerado lá dentro e salve o arquivo.

Pronto! Agora o aplicativo terá permissão para se conectar ao seu Dropbox e baixar seus arquivos.

🛠️ Bibliotecas Utilizadas
Este projeto utiliza várias bibliotecas open-source poderosas para funcionar:

stb_image.h - Para carregamento de imagens.

stb_truetype.h - Para renderização de fontes TTF.

miniz - Para compressão/descompressão de dados.

dr_mp3 e dr_wav - Para decodificação e reprodução de áudio.

Desenvolvido para a comunidade Homebrew. Sinta-se à vontade para abrir Issues relatando bugs ou enviar Pull Requests com melhorias!
