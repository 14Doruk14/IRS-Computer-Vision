/*
 * PDF kavramsal sorulari icin deneyler (Code::Blocks projesine EKLENMEZ, ayri derlenir).
 *   Soru 1: kernel boyutunun etkisi  -> kernel_1/3/5/12.pgm
 *   Soru 2: cift maske               -> maske_mavi/beyaz/cift.pgm
 * Proje klasorunden calistirilir: arena.ppm'i okur, ciktilari deneyler/ icine yazar.
 */

#include <stdio.h>
#include "../irs_vision.h"

int main(void)
{
    int NC, NR;
    color **img = ppm_file_to_img("arena.ppm", &NC, &NR);

    HsvAralik mavi  = {100, 40, 120, 140, 255, 255};
    HsvAralik beyaz = {  0,  0, 240, 180,  30, 255};
    SekilFiltre isik_cubugu = {0.25, 50.0, 200.0, 200.0};

    /* ---------- Soru 2: tek maske vs cift maske ---------- */
    unsigned char **m_mavi  = hsv_maske(img, NC, NR, mavi);
    unsigned char **m_beyaz = hsv_maske(img, NC, NR, beyaz);
    unsigned char **m_cift  = hsv_maske(img, NC, NR, mavi);
    maske_or(m_cift, m_beyaz, NC, NR);

    img_to_pgm_file(m_mavi,  "deneyler/maske_mavi.pgm",  NC, NR);
    img_to_pgm_file(m_beyaz, "deneyler/maske_beyaz.pgm", NC, NR);
    img_to_pgm_file(m_cift,  "deneyler/maske_cift.pgm",  NC, NR);

    printf("=== Soru 2: maske karsilastirmasi (isik cubugu bolgesi x=940-1110, y=530-555) ===\n");
    const char *adlar[3] = { "sadece mavi ", "sadece beyaz", "mavi+beyaz  " };
    unsigned char **maskeler[3] = { m_mavi, m_beyaz, m_cift };

    for (int m = 0; m < 3; m++)
    {
        int dolu = 0, toplam = 0;
        for (int y = 530; y < 555; y++)
            for (int x = 940; x < 1110; x++)
            {
                toplam++;
                if (maskeler[m][y][x] == 255) dolu++;
            }
        printf("%s: isik bolgesinin %%%2d'i maskede\n", adlar[m], 100 * dolu / toplam);
    }

    /* ---------- Soru 1: kernel boyutu ---------- */
    printf("\n=== Soru 1: kernel boyutu ===\n");
    int kerneller[4] = { 1, 3, 5, 12 };

    for (int i = 0; i < 4; i++)
    {
        int k = kerneller[i];
        unsigned char **temiz = acma(m_cift, NC, NR, k);

        char dosya[64];
        sprintf(dosya, "deneyler/kernel_%d.pgm", k);
        img_to_pgm_file(temiz, dosya, NC, NR);

        int beyaz_piksel = 0;
        for (int y = 0; y < NR; y++)
            for (int x = 0; x < NC; x++)
                if (temiz[y][x] == 255) beyaz_piksel++;

        Bolge *b;
        int n = bolgeleri_bul(temiz, NC, NR, &b);
        int n_once = n;
        bolgeleri_birlestir(b, &n, 5.0, 15.0, 6.0);

        int bulunan = 0;
        double en_uzun = 0;
        for (int j = 0; j < n; j++)
        {
            parlaklik_hesapla(img, NC, NR, &b[j]);
            if (sekle_uyar_mi(&b[j], isik_cubugu)) bulunan++;
            if (b[j].uzun > en_uzun) en_uzun = b[j].uzun;
        }

        printf("kernel %2d: beyaz piksel=%6d  parca=%4d  birlesik=%4d  en uzun=%6.1f  isik cubugu=%d\n",
               k, beyaz_piksel, n_once, n, en_uzun, bulunan);

        free(b);
        free_img(temiz);
    }

    free_img(m_mavi);
    free_img(m_beyaz);
    free_img(m_cift);
    free_color_img(img);
    return 0;
}
