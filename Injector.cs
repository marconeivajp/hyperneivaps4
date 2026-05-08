using System;
using System.IO;
using System.Text;

class Injector {
    static void Main() {
        string path = @"C:\Users\Marco\source\repos\teste3\teste3\controle_explorar.cpp";
        string content = File.ReadAllText(path, Encoding.GetEncoding(1252));

        string extOld = "extern void acaoArquivo(int idxOpcao);";
        string extNew = "extern void acaoArquivo(int idxOpcao);\r\nextern void iniciarEmulador(const char* romPath);\r\nextern char msgStatus[128];\r\nextern int msgTimer;";

        if (content.Contains(extOld) && !content.Contains("iniciarEmulador")) {
            content = content.Replace(extOld, extNew);
            Console.WriteLine("Externs added.");
        }

        string logicOld = "else if (strstr(nomeBlindado, \".mp4\")) {\r\n                iniciarVideoMP4(caminhoArquivo);\r\n            }";
        // Fallback for different line endings
        if (!content.Contains(logicOld)) {
            logicOld = "else if (strstr(nomeBlindado, \".mp4\")) {\n                iniciarVideoMP4(caminhoArquivo);\n            }";
        }

        string logicNew = "else if (strstr(nomeBlindado, \".mp4\")) {\r\n                iniciarVideoMP4(caminhoArquivo);\r\n            }\r\n            else if (strstr(nomeBlindado, \".bin\") || strstr(nomeBlindado, \".zip\") || strstr(nomeBlindado, \".md\") || strstr(nomeBlindado, \".smd\") || strstr(nomeBlindado, \".gen\") || strstr(nomeBlindado, \".rar\") || strstr(nomeBlindado, \".7z\")) {\r\n                snprintf(msgStatus, sizeof(msgStatus), \"ROM Detectada!\"); msgTimer = 120;\r\n                iniciarEmulador(caminhoArquivo);\r\n            }";

        if (content.Contains(logicOld) && !content.Contains("ROM Detectada")) {
            content = content.Replace(logicOld, logicNew);
            Console.WriteLine("Logic added.");
        } else {
            Console.WriteLine("Could not find logicOld block. String search failed.");
            // Last resort: search just part of it
             string logicOldShort = "else if (strstr(nomeBlindado, \".mp4\")) {";
             if (content.Contains(logicOldShort) && !content.Contains("ROM Detectada")) {
                 content = content.Replace(logicOldShort, "else if (strstr(nomeBlindado, \".mp4\")) {\r\n                iniciarVideoMP4(caminhoArquivo);\r\n            }\r\n            else if (strstr(nomeBlindado, \".bin\") || strstr(nomeBlindado, \".md\") || strstr(nomeBlindado, \".smd\") || strstr(nomeBlindado, \".gen\") || strstr(nomeBlindado, \".zip\") || strstr(nomeBlindado, \".rar\") || strstr(nomeBlindado, \".7z\")) {\r\n                snprintf(msgStatus, sizeof(msgStatus), \"ROM Detectada!\"); msgTimer = 120;\r\n                iniciarEmulador(caminhoArquivo);\r\n            }\r\n            if (0) {");
                 Console.WriteLine("Logic added via short search.");
             }
        }

        File.WriteAllText(path, content, Encoding.UTF8);
        Console.WriteLine("File saved as UTF-8.");
    }
}
