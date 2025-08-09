#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <locale.h>

#define STRLEN_UF   3
#define STRLEN_COD  8
#define STRLEN_CID  80

typedef struct {
    char estado[STRLEN_UF];        // ex: "CE"
    char codigo[STRLEN_COD];       // ex: "A01"
    char cidade[STRLEN_CID];       // ex: "Mucambo"
    unsigned long long populacao;  // habitantes
    double area;                    // km^2
    double pib;                     // PIB (unidade livre, ex: bilhões ou absoluto)
    unsigned int pontosTuristicos;  // contagem
} Carta;

typedef enum {
    ATR_INVALIDO = 0,
    ATR_POPULACAO = 1,
    ATR_AREA = 2,
    ATR_PIB = 3,
    ATR_PONTOS_TURISTICOS = 4,
    ATR_DENSIDADE = 5,        // menor vence
    ATR_PIB_PER_CAPITA = 6    // maior vence
} Atributo;

typedef struct {
    Atributo id;
    const char* nome;
    const char* unidade;     // só para exibição
    bool menorVence;
    bool inteiro;            // para comparação sem epsilon
} AtributoInfo;

static const AtributoInfo ATRIBUTOS[] = {
    { ATR_INVALIDO,        "Inválido",           "",            false, false },
    { ATR_POPULACAO,       "População",          "hab",         false, true  },
    { ATR_AREA,            "Área",               "km^2",        false, false },
    { ATR_PIB,             "PIB",                "",            false, false },
    { ATR_PONTOS_TURISTICOS,"Pontos turísticos", "pontos",      false, true  },
    { ATR_DENSIDADE,       "Densidade",          "hab/km^2",    true,  false },
    { ATR_PIB_PER_CAPITA,  "PIB per capita",     "",            false, false }
};

static void trim_nl(char* s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = '\0';
}

static void to_upper_ascii(char* s) {
    if (!s) return;
    for (; *s; ++s) *s = (char)toupper((unsigned char)*s);
}

static void clear_stdin(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { }
}

static bool read_line(const char* prompt, char* buf, size_t bufsz) {
    if (prompt) printf("%s", prompt);
    if (!fgets(buf, (int)bufsz, stdin)) return false;
    trim_nl(buf);
    return true;
}

static void replace_commas_with_dots(char* s) {
    if (!s) return;
    for (; *s; ++s) if (*s == ',') *s = '.';
}

static bool parse_double_str(const char* s, double* out) {
    if (!s || !out) return false;
    char tmp[256];
    strncpy(tmp, s, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = '\0';
    replace_commas_with_dots(tmp);
    char* endptr = NULL;
    double v = strtod(tmp, &endptr);
    if (endptr == tmp || *endptr != '\0') return false;
    *out = v;
    return true;
}

static bool parse_ull_str(const char* s, unsigned long long* out) {
    if (!s || !out) return false;
    // Ignora espaços
    while (isspace((unsigned char)*s)) s++;
    if (!*s) return false;
    char* endptr = NULL;
    unsigned long long v = strtoull(s, &endptr, 10);
    if (endptr == s || *endptr != '\0') return false;
    *out = v;
    return true;
}

static bool parse_uint_str(const char* s, unsigned int* out) {
    unsigned long long tmp = 0;
    if (!parse_ull_str(s, &tmp)) return false;
    if (tmp > UINT_MAX) return false;
    *out = (unsigned int)tmp;
    return true;
}

static double densidade(const Carta* c) {
    if (!c) return NAN;
    if (c->area <= 0.0) return INFINITY; // área zero => densidade "infinita"
    return (double)((long double)c->populacao / (long double)c->area);
}

static double pib_per_capita(const Carta* c) {
    if (!c) return NAN;
    if (c->populacao == 0ULL) return 0.0; // evita divisão por zero
    return (double)((long double)c->pib / (long double)c->populacao);
}

static const AtributoInfo* get_attr_info(Atributo a) {
    if (a < ATR_INVALIDO || a > ATR_PIB_PER_CAPITA) return &ATRIBUTOS[0];
    return &ATRIBUTOS[a];
}

static long double attr_value_ld(const Carta* c, Atributo a) {
    switch (a) {
        case ATR_POPULACAO:        return (long double)c->populacao;
        case ATR_AREA:             return (long double)c->area;
        case ATR_PIB:              return (long double)c->pib;
        case ATR_PONTOS_TURISTICOS:return (long double)c->pontosTuristicos;
        case ATR_DENSIDADE:        return (long double)densidade(c);
        case ATR_PIB_PER_CAPITA:   return (long double)pib_per_capita(c);
        default:                   return NAN;
    }
}

static bool is_nan_or_inf(long double v) {
    return isnan((double)v) || isinf((double)v);
}

// Comparação robusta para doubles com tolerância relativa.
static int cmp_long_double(long double a, long double b, bool integer) {
    if (integer) {
        if (a < b) return -1;
        if (a > b) return 1;
        return 0;
    }
    if (is_nan_or_inf(a) || is_nan_or_inf(b)) {
        if (is_nan_or_inf(a) && is_nan_or_inf(b)) return 0;
        return is_nan_or_inf(a) ? 1 : -1; // trata NaN/Inf como “maior” para não favorecer
    }
    long double diff = fabsl(a - b);
    long double scale = fmaxl(1.0L, fmaxl(fabsl(a), fabsl(b)));
    long double eps = 1e-9L * scale;
    if (diff <= eps) return 0;
    return (a < b) ? -1 : 1;
}

static void imprimir_carta(const Carta* c) {
    if (!c) return;
    printf("--------------------------------------------------\n");
    printf("Estado: %s | Código: %s | Cidade: %s\n", c->estado, c->codigo, c->cidade);
    printf("- População: %llu hab\n", (unsigned long long)c->populacao);
    printf("- Área: %.3f km^2\n", c->area);
    printf("- PIB: %.3f\n", c->pib);
    printf("- Pontos turísticos: %u\n", c->pontosTuristicos);
    printf("- Densidade: %.6f hab/km^2\n", densidade(c));
    printf("- PIB per capita: %.6f\n", pib_per_capita(c));
}

static void imprimir_cartas(const Carta* a, const Carta* b) {
    printf("\n=== Cartas Cadastradas ===\n");
    printf("[Carta 1]\n"); imprimir_carta(a);
    printf("[Carta 2]\n"); imprimir_carta(b);
    printf("--------------------------------------------------\n\n");
}

static void listar_atributos_dinamico(Atributo exclui) {
    printf("\nEscolha um atributo:\n");
    for (int i = ATR_POPULACAO; i <= ATR_PIB_PER_CAPITA; ++i) {
        if ((Atributo)i == exclui) continue;
        const AtributoInfo* info = get_attr_info((Atributo)i);
        printf("%d) %s%s%s %s\n",
               i,
               info->nome,
               info->unidade[0] ? " (" : "",
               info->unidade[0] ? info->unidade : "",
               info->menorVence ? "[menor vence]" : "");
    }
    printf("Opção: ");
}

static Atributo ler_opcao_atributo(Atributo exclui) {
    char buf[64];
    while (true) {
        listar_atributos_dinamico(exclui);
        if (!read_line("", buf, sizeof(buf))) return ATR_INVALIDO;
        int op = atoi(buf);
        if (op >= ATR_POPULACAO && op <= ATR_PIB_PER_CAPITA && (Atributo)op != exclui) {
            return (Atributo)op;
        }
        printf("Opção inválida. Tente novamente.\n");
    }
}

static void cadastrar_carta(Carta* c, const char* titulo) {
    printf("\n=== Cadastro %s ===\n", titulo);

    char buf[256];

    // Estado (2 letras)
    while (true) {
        if (!read_line("Estado (UF, 2 letras): ", buf, sizeof(buf))) exit(1);
        trim_nl(buf);
        if (strlen(buf) == 2) {
            to_upper_ascii(buf);
            strncpy(c->estado, buf, STRLEN_UF);
            c->estado[2] = '\0';
            break;
        }
        printf("UF inválida. Digite 2 letras, ex: CE.\n");
    }

    // Código da carta
    while (true) {
        if (!read_line("Código da carta (ex: A01): ", buf, sizeof(buf))) exit(1);
        if (strlen(buf) >= 1 && strlen(buf) < STRLEN_COD) {
            strncpy(c->codigo, buf, STRLEN_COD);
            c->codigo[STRLEN_COD-1] = '\0';
            break;
        }
        printf("Código inválido. Tamanho máximo %d.\n", STRLEN_COD-1);
    }

    // Nome da cidade
    while (true) {
        if (!read_line("Nome da cidade: ", buf, sizeof(buf))) exit(1);
        if (strlen(buf) >= 1 && strlen(buf) < STRLEN_CID) {
            strncpy(c->cidade, buf, STRLEN_CID);
            c->cidade[STRLEN_CID-1] = '\0';
            break;
        }
        printf("Nome muito longo. Tamanho máximo %d.\n", STRLEN_CID-1);
    }

    // População
    while (true) {
        if (!read_line("População (inteiro >= 0): ", buf, sizeof(buf))) exit(1);
        unsigned long long v = 0;
        if (parse_ull_str(buf, &v)) {
            c->populacao = v;
            break;
        }
        printf("Valor inválido.\n");
    }

    // Área
    while (true) {
        if (!read_line("Área em km^2 (>= 0): ", buf, sizeof(buf))) exit(1);
        double v = 0.0;
        if (parse_double_str(buf, &v) && v >= 0.0) {
            c->area = v;
            break;
        }
        printf("Valor inválido.\n");
    }

    // PIB
    while (true) {
        if (!read_line("PIB (>= 0): ", buf, sizeof(buf))) exit(1);
        double v = 0.0;
        if (parse_double_str(buf, &v) && v >= 0.0) {
            c->pib = v;
            break;
        }
        printf("Valor inválido.\n");
    }

    // Pontos turísticos
    while (true) {
        if (!read_line("Pontos turísticos (inteiro >= 0): ", buf, sizeof(buf))) exit(1);
        unsigned int v = 0;
        if (parse_uint_str(buf, &v)) {
            c->pontosTuristicos = v;
            break;
        }
        printf("Valor inválido.\n");
    }
}

static void exibir_valor(const Carta* c, Atributo a) {
    const AtributoInfo* info = get_attr_info(a);
    long double v = attr_value_ld(c, a);
    if (info->inteiro)
        printf("%s: %llu %s", info->nome, (unsigned long long)llround((long long)v), info->unidade);
    else
        printf("%s: %.6Lf %s", info->nome, v, info->unidade);
}

static void explicar_resultado(const Carta* c1, const Carta* c2, Atributo a1, Atributo a2) {
    const AtributoInfo* i1 = get_attr_info(a1);
    const AtributoInfo* i2 = get_attr_info(a2);
    printf("\nDetalhes da comparação:\n");
    printf("- Atributo primário (%s %s):\n", i1->nome, i1->menorVence ? "[menor vence]" : "[maior vence]");
    printf("  Carta 1 -> "); exibir_valor(c1, a1); printf("\n");
    printf("  Carta 2 -> "); exibir_valor(c2, a1); printf("\n");
    printf("- Atributo secundário (%s %s):\n", i2->nome, i2->menorVence ? "[menor vence]" : "[maior vence]");
    printf("  Carta 1 -> "); exibir_valor(c1, a2); printf("\n");
    printf("  Carta 2 -> "); exibir_valor(c2, a2); printf("\n");
}

static void comparar_duplo(const Carta* c1, const Carta* c2, Atributo prim, Atributo sec) {
    const AtributoInfo* ip = get_attr_info(prim);
    const AtributoInfo* isec = get_attr_info(sec);

    long double v1p = attr_value_ld(c1, prim);
    long double v2p = attr_value_ld(c2, prim);
    int cmpP = cmp_long_double(v1p, v2p, ip->inteiro);

    // Decisão primária com operador ternário:
    int vencedorPrimario = (cmpP == 0) ? -1 : (ip->menorVence ? (cmpP < 0 ? 1 : 2) : (cmpP > 0 ? 1 : 2));
    // -1 significa "empate no primário"

    int vencedorFinal = 0;
    if (vencedorPrimario == -1) {
        // Desempate pelo secundário
        long double v1s = attr_value_ld(c1, sec);
        long double v2s = attr_value_ld(c2, sec);
        int cmpS = cmp_long_double(v1s, v2s, isec->inteiro);
        vencedorFinal = (cmpS == 0) ? 0 : (isec->menorVence ? (cmpS < 0 ? 1 : 2) : (cmpS > 0 ? 1 : 2));
    } else {
        vencedorFinal = vencedorPrimario;
    }

    explicar_resultado(c1, c2, prim, sec);

    if (vencedorFinal == 0) {
        printf("\nResultado: EMPATE! As cartas são equivalentes nos dois atributos.\n");
    } else {
        const Carta* vencedora = (vencedorFinal == 1) ? c1 : c2;
        const Carta* perdedora = (vencedorFinal == 1) ? c2 : c1;
        printf("\nResultado: Carta %d venceu! (%s - %s)\n",
               vencedorFinal, vencedora->codigo, vencedora->cidade);

        // Justificativa curta
        if (vencedorPrimario == -1) {
            printf("Desempate decidido pelo atributo secundário.\n");
        } else {
            printf("Vitória decidida pelo atributo primário.\n");
        }

        // Extra: destacar diferenças numéricas
        printf("\nDiferenças numéricas:\n");
        long double dvp = fabsl(v1p - v2p);
        long double v1s = attr_value_ld(c1, sec);
        long double v2s = attr_value_ld(c2, sec);
        long double dvs = fabsl(v1s - v2s);

        printf("- |%s(C1) - %s(C2)| = %.6Lf\n", get_attr_info(prim)->nome, get_attr_info(prim)->nome, dvp);
        printf("- |%s(C1) - %s(C2)| = %.6Lf\n", get_attr_info(sec)->nome,  get_attr_info(sec)->nome,  dvs);
    }
    printf("\n");
}

static void menu_comparar(const Carta* c1, const Carta* c2) {
    if (!c1 || !c2) { printf("Cadastre as cartas antes de comparar.\n"); return; }
    printf("\n=== Comparação por Dois Atributos ===\n");
    Atributo prim = ler_opcao_atributo(ATR_INVALIDO);
    Atributo sec = ler_opcao_atributo(prim);
    comparar_duplo(c1, c2, prim, sec);
}

static int ler_menu_principal(void) {
    printf("===== Super Trunfo — Países (Cidades) =====\n");
    printf("1) Cadastrar cartas\n");
    printf("2) Exibir cartas\n");
    printf("3) Comparar (dois atributos)\n");
    printf("0) Sair\n");
    printf("Opção: ");
    char buf[32];
    if (!read_line("", buf, sizeof(buf))) return 0;
    return atoi(buf);
}

int main(void) {
    setlocale(LC_ALL, "");

    Carta c1 = {0}, c2 = {0};
    bool cadastradas = false;

    while (1) {
        int op = ler_menu_principal();
        switch (op) {
            case 1:
                cadastrar_carta(&c1, "da Carta 1");
                cadastrar_carta(&c2, "da Carta 2");
                cadastradas = true;
                printf("\nCartas cadastradas com sucesso!\n\n");
                break;
            case 2:
                if (!cadastradas) {
                    printf("\nNenhuma carta cadastrada ainda. Use a opção 1 primeiro.\n\n");
                } else {
                    imprimir_cartas(&c1, &c2);
                }
                break;
            case 3:
                if (!cadastradas) {
                    printf("\nNenhuma carta cadastrada ainda. Use a opção 1 primeiro.\n\n");
                } else {
                    menu_comparar(&c1, &c2);
                }
                break;
            case 0:
                printf("Até a próxima!\n");
                return 0;
            default:
                printf("Opção inválida. Tente novamente.\n\n");
        }
    }
    return 0;
}
