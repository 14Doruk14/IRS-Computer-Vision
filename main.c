/*
 * IRS Gorev 2: Robot uzerindeki yatay mavi isik cubugunu bul ve yesil kutuyla goster.
 * Tum goruntu isleme fonksiyonlari irs_vision.c / irs_vision.h icinde.
 */

#include <stdio.h>
#include "irs_vision.h"

/* ---------------- Gorev parametreleri ---------------- */

/* Colab: kernel = np.ones((3,3)) */
#define KERNEL 3

int main(void)
{
    int NC, NR;
    color **img = ppm_file_to_img("arena.ppm", &NC, &NR);

    printf("Genislik: %d piksel\n", NC);
    printf("Yukseklik: %d piksel\n", NR);

    /* Colab: lower/upper araliklari  {hmin, smin, vmin, hmax, smax, vmax} */
    HsvAralik mavi  = {100, 40, 120, 140, 255, 255};
    HsvAralik beyaz = {  0,  0, 240, 180,  30, 255};

    /* {max_oran, min_uzun, max_uzun, min_parlaklik, min_piksel}
       Colab: 0.25, 50, 110, yok, yok -> birlesik cubuk ~154 px oldugu icin 200'e cikti */
    SekilFiltre isik_cubugu = {0.25, 50.0, 200.0, 200.0, 50};

    color yesil = {0, 255, 0};

    /* ---------------- 1) Renk maskesi (cift maske) ---------------- */
    unsigned char **mask        = hsv_maske(img, NC, NR, mavi);
    unsigned char **beyaz_maske = hsv_maske(img, NC, NR, beyaz);

    maske_or(mask, beyaz_maske, NC, NR);   /* mask = mavi VEYA beyaz */
    free_img(beyaz_maske);

    img_to_pgm_file(mask, "gurultulu_maske.pgm", NC, NR);
    printf("Maske kaydedildi.\n");

    /* ---------------- 2) GOREV 1: morfolojik acma ---------------- */
    unsigned char **cleaned = acma(mask, NC, NR, KERNEL);

    img_to_pgm_file(cleaned, "temizlenmis_maske.pgm", NC, NR);
    printf("Temizlenmis maske kaydedildi (kernel %dx%d).\n", KERNEL, KERNEL);

    /* ---------------- 3) GOREV 2: parcalar + filtre + kutu ---------------- */
    Bolge *bolgeler;
    int n = bolgeleri_bul(cleaned, NC, NR, &bolgeler);
    printf("%d parca bulundu.\n", n);

    /* alan filtresi: kucuk gurultu parcalarini birlestirmeden ONCE at */
    kucuk_bolgeleri_sil(bolgeler, &n, isik_cubugu.min_piksel);
    printf("Kucukler silindikten sonra %d parca.\n", n);

    /* bolunmus cubuklari birlestir: aci farki < 5 derece, bosluk < 15 px, kayma < 6 px */
    bolgeleri_birlestir(bolgeler, &n, 5.0, 15.0, 6.0);
    printf("Birlestirmeden sonra %d parca.\n\n", n);

    /* parlakliklari kutular cizilmeden ONCE hesapla */
    for (int i = 0; i < n; i++)
        parlaklik_hesapla(img, NC, NR, &bolgeler[i]);

    int adet = 0;

    for (int i = 0; i < n; i++)
    {
        Bolge *b = &bolgeler[i];
        int gecti = sekle_uyar_mi(b, isik_cubugu);

        printf("Parca %2d: x=%4d y=%4d uzun=%6.1f kisa=%5.1f oran=%.2f aci=%6.1f parlak=%3.0f piksel=%4d %s\n",
               i + 1, b->x, b->y, b->uzun, b->kisa,
               b->uzun > 0 ? b->kisa / b->uzun : 0.0, b->aci, b->parlaklik, b->piksel,
               gecti ? "<-- ISIK CUBUGU" : "");

        if (gecti)
        {
            kutu_ciz(img, NC, NR, b, yesil);
            adet++;
        }
    }

    printf("\n%d isik cubugu bulundu.\n", adet);

    img_to_ppm_file(img, "sonuc.ppm", NC, NR);
    printf("Sonuc kaydedildi: sonuc.ppm\n");

    /* ---------------- Temizlik ---------------- */
    free(bolgeler);
    free_img(cleaned);
    free_img(mask);
    free_color_img(img);

    return 0;
}
