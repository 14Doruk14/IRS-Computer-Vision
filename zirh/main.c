/*
 * IRS Zirh Tespiti: zirh plakasinin iki yanindaki dikey isiklari bulup eslestirir,
 * zirhin yerini ve nisan noktasini (merkez) isaretler.
 *
 * Kutuphane: ../irs_vision.c, ../irs_vision.h, ../img_pro.c, ../img_pro.h
 */

#include <stdio.h>
#include "../irs_vision.h"

/* ---------------- Gorev parametreleri ---------------- */

#define GIRDI   "../arena.ppm"
#define CIKTI   "zirh_sonuc.ppm"

#define HEDEF_RENK  RENK_MAVI   /* karsi takimin rengi: RENK_MAVI veya RENK_KIRMIZI */

int main(void)
{
    int NC, NR;
    color **img = ppm_file_to_img(GIRDI, &NC, &NR);
    printf("Resim: %d x %d\n", NC, NR);

    /* 1) Parlaklik maskesi: renge bakmadan sadece cok parlak pikseller.
          Isigin cekirdegi doymus beyazdir -> ince ve keskin cubuk verir.
          (Mavi hale ise etrafa yayilip cubugu sisman lekeye cevirir.) */
    HsvAralik parlak = {0, 0, 230, 180, 255, 255};

    /* 2) Dikey isik filtresi
          {max_oran, min_uzun, max_uzun, min_parlaklik, min_piksel, hedef_aci, max_aci_sapma} */
    SekilFiltre dikey_isik = {0.5, 10.0, 300.0, 0.0, 20, 90.0, 30.0};

    /* 3) Isik eslestirme kurallari
          {max_aci_farki, min_boy_orani, max_kayma, min_mesafe, max_mesafe} */
    ZirhFiltre zirh_kurali = {10.0, 0.6, 0.5, 1.5, 6.0};

    color sari    = {255, 255, 0};
    color yesil   = {0, 255, 0};

    /* ---------------- Maske ve parcalar ---------------- */
    unsigned char **mask = hsv_maske(img, NC, NR, parlak);
    img_to_pgm_file(mask, "parlak_maske.pgm", NC, NR);

    Bolge *bolgeler;
    int n = bolgeleri_bul(mask, NC, NR, &bolgeler);
    kucuk_bolgeleri_sil(bolgeler, &n, dikey_isik.min_piksel);
    printf("%d parca (kucukler silindi)\n\n", n);

    /* ---------------- Dikey isiklari sec ---------------- */
    Bolge *isiklar = malloc((n > 0 ? n : 1) * sizeof(Bolge));
    int isik_sayisi = 0;

    for (int i = 0; i < n; i++)
    {
        if (!sekle_uyar_mi(&bolgeler[i], dikey_isik))
            continue;

        renk_hesapla(img, NC, NR, &bolgeler[i]);   /* cizimden ONCE */
        isiklar[isik_sayisi++] = bolgeler[i];

        Bolge *b = &bolgeler[i];
        printf("Isik %d: x=%4d y=%4d uzun=%5.1f kisa=%4.1f aci=%6.1f renk=%s\n",
               isik_sayisi, b->x, b->y, b->uzun, b->kisa, b->aci,
               b->renk == RENK_MAVI ? "mavi" : "kirmizi");
    }

    /* ---------------- Isiklari eslestir -> zirh ---------------- */
    Zirh *zirhlar;
    int zirh_sayisi = zirh_bul(isiklar, isik_sayisi, zirh_kurali, &zirhlar);

    /* once tum isiklari sari ciz (eslesmeyenler de gorunsun) */
    for (int i = 0; i < isik_sayisi; i++)
        kutu_ciz(img, NC, NR, &isiklar[i], sari);

    int hedef = 0;
    printf("\n");
    for (int i = 0; i < zirh_sayisi; i++)
    for (int i = 0; i < zirh_sayisi; i++)
    {
        Zirh *z = &zirhlar[i];
        printf("Zirh %d: merkez=(%.0f, %.0f) mesafe_orani=%.2f renk=%s %s\n",
               i + 1, z->cx, z->cy, z->mesafe_orani,
               z->renk == RENK_MAVI ? "mavi" : "kirmizi",
               z->renk == HEDEF_RENK ? "<-- HEDEF" : "(bizim takim, atla)");

        if (z->renk == HEDEF_RENK)
        {
            zirh_ciz(img, NC, NR, z, yesil);
            hedef++;
        }
    }

    printf("\n%d dikey isik, %d zirh, %d hedef.\n", isik_sayisi, zirh_sayisi, hedef);

    img_to_ppm_file(img, CIKTI, NC, NR);
    printf("Sonuc kaydedildi: %s\n", CIKTI);

    /* ---------------- Temizlik ---------------- */
    free(zirhlar);
    free(isiklar);
    free(bolgeler);
    free_img(mask);
    free_color_img(img);
    return 0;
}
