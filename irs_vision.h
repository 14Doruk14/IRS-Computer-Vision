/*
 * irs_vision.h - IRS goruntu isleme kutuphanesi
 *
 * Bu dosya "icindekiler" sayfasidir: struct'lar ve fonksiyonlarin imzalari.
 * Fonksiyonlarin kodu irs_vision.c icindedir.
 *
 * Kullanim:  #include "irs_vision.h"
 * Projeye eklenmesi gerekenler: irs_vision.c, irs_vision.h, img_pro.c, img_pro.h
 */

#ifndef IRS_VISION_H       /* include guard: bu dosya iki kez eklenirse */
#define IRS_VISION_H       /* ikinci seferde icerigi atlanir             */

#include "img_pro.h"       /* color, alloc_img, free_img, ppm/pgm okuma-yazma */

/* ------------------------------------------------------------------ */
/*  Veri tipleri                                                        */
/* ------------------------------------------------------------------ */

/* HSV renk araligi (OpenCV olcegi: H 0-180, S 0-255, V 0-255)
   Colab: lower = [hmin, smin, vmin], upper = [hmax, smax, vmax] */
typedef struct {
    int hmin;
    int smin;
    int vmin;
    int hmax;
    int smax;
    int vmax;
} HsvAralik;

/* Bir parcanin olculeri (cv2.minAreaRect sonucunun karsiligi) */
typedef struct {
    double cx, cy;          /* merkez */
    double uzun, kisa;      /* dondurulmus kutunun kenarlari */
    double aci;             /* uzun kenarin acisi (derece): 0 yatay, +-90 dikey */
    double kose[4][2];      /* 4 kose: kose[i][0]=x, kose[i][1]=y */
    int piksel;             /* parcadaki piksel sayisi */
    int x, y, en, boy;      /* eksene hizali kutu (sol-ust, genislik, yukseklik) */
    double parlaklik;       /* kutu icindeki ortalama V (0-255), parlaklik_hesapla doldurur */
    int renk;               /* RENK_MAVI / RENK_KIRMIZI / RENK_YOK, renk_hesapla doldurur */
} Bolge;

/* Isik rengi (takim rengi) */
#define RENK_YOK     (-1)
#define RENK_MAVI      0
#define RENK_KIRMIZI   1

/* Filtre: kisa/uzun < max_oran, min_uzun < uzun < max_uzun,
           parlaklik >= min_parlaklik, piksel >= min_piksel,
           aci hedef_aci'dan en fazla max_aci_sapma kadar sapmis */
typedef struct {
    double max_oran;
    double min_uzun;
    double max_uzun;
    double min_parlaklik;   /* 0 = parlaklik kontrolu yok */
    int    min_piksel;      /* 0 = alan kontrolu yok (yazilmazsa otomatik 0) */
    double hedef_aci;       /* aranan yon (derece): 0 yatay, 90 dikey */
    double max_aci_sapma;   /* 0 = aci kontrolu yok (yazilmazsa otomatik 0) */
} SekilFiltre;

/* Zirh plakasi: iki yanindaki dikey isik cubugundan bulunur */
typedef struct {
    Bolge sol, sag;         /* soldaki ve sagdaki isik */
    double kose[4][2];      /* isik uclari: 0 sol-ust, 1 sag-ust, 2 sag-alt, 3 sol-alt */
    double cx, cy;          /* zirhin merkezi (nisan noktasi) */
    double mesafe_orani;    /* isiklar arasi mesafe / ortalama isik boyu (kucuk ~2-3, buyuk ~4-5) */
    int renk;               /* iki isigin ortak rengi */
} Zirh;

/* Iki isik ayni zirhin isiklari mi? */
typedef struct {
    double max_aci_farki;   /* iki isik paralel olmali (derece) */
    double min_boy_orani;   /* kisa isik / uzun isik >= bu (benzer boyda olmali) */
    double max_kayma;       /* isik yonunde kayma / ortalama boy <= bu (ayni hizada olmali) */
    double min_mesafe;      /* aradaki mesafe / ortalama boy >= bu */
    double max_mesafe;      /* aradaki mesafe / ortalama boy <= bu */
} ZirhFiltre;

/* ------------------------------------------------------------------ */
/*  Renk                                                                */
/* ------------------------------------------------------------------ */

/* RGB -> HSV (OpenCV olcegi). Sonuclar h, s, v adreslerine yazilir. */
void rgb_to_hsv(color pixel, float *h, float *s, float *v);

/* cv2.inRange: araliktaki piksellere 255, digerlerine 0. YENI bellek -> free_img */
unsigned char **hsv_maske(color **img, int NC, int NR, HsvAralik a);

/* cv2.bitwise_or: ekle'deki beyazlari hedef'e de yazar (hedef degisir) */
void maske_or(unsigned char **hedef, unsigned char **ekle, int NC, int NR);

/* ------------------------------------------------------------------ */
/*  Morfoloji (k = kernel boyutu, tek sayi: 3, 5, 7 ...)                */
/* ------------------------------------------------------------------ */

unsigned char **erode(unsigned char **src, int NC, int NR, int k);   /* YENI bellek */
unsigned char **dilate(unsigned char **src, int NC, int NR, int k);  /* YENI bellek */
unsigned char **acma(unsigned char **src, int NC, int NR, int k);    /* erode+dilate, YENI bellek */

/* ------------------------------------------------------------------ */
/*  Parcalar (bolgeler)                                                 */
/* ------------------------------------------------------------------ */

/* cv2.findContours + cv2.minAreaRect: maskedeki beyaz parcalari bulur.
   *liste YENI dizi -> isin bitince free(*liste). Donus: parca sayisi. */
int bolgeleri_bul(unsigned char **src, int NC, int NR, Bolge **liste);

/* Piksel listesinden dondurulmus kutu hesaplar. pixels[i] = y * NC + x */
void bolge_hesapla(const int *pixels, int count, int NC, Bolge *b);

/* min_piksel'den kucuk parcalari listeden siler (alan filtresi). *n azalir.
   Birlestirmeden ONCE cagrilirsa gurultu birlestirmeye de karismaz. */
void kucuk_bolgeleri_sil(Bolge *b, int *n, int min_piksel);

/* Ayni dogru uzerinde, ayni acida ve yakin parcalari birlestirir. *n azalir. */
void bolgeleri_birlestir(Bolge *b, int *n,
                         double max_aci, double max_bosluk, double max_kayma);

/* Iki bolgeyi tek bolgede birlestirir */
Bolge bolge_birlestir(const Bolge *b1, const Bolge *b2);

/* Iki aci arasindaki fark (derece, 0..90). 179 ile -1 ayni yondur. */
double aci_farki(double a1, double a2);

/* Kutu icindeki ortalama parlakligi b->parlaklik'a yazar. Kutular CIZILMEDEN ONCE cagir. */
void parlaklik_hesapla(color **img, int NC, int NR, Bolge *b);

/* Bolge filtreye uyuyor mu? 1 = evet, 0 = hayir */
int sekle_uyar_mi(const Bolge *b, SekilFiltre f);

/* ------------------------------------------------------------------ */
/*  Cizim                                                               */
/* ------------------------------------------------------------------ */

/* 2 piksel kalin cizgi (Bresenham) */
void draw_line(color **img, int NC, int NR,
               int x0, int y0, int x1, int y1, color c);

/* Bolgenin dondurulmus kutusunu cizer (cv2.drawContours) */
void kutu_ciz(color **img, int NC, int NR, const Bolge *b, color c);

/* ------------------------------------------------------------------ */
/*  Zirh tespiti                                                        */
/* ------------------------------------------------------------------ */

/* Isigin etrafindaki mavi ve kirmiziyi karsilastirip b->renk'e yazar.
   Kutular CIZILMEDEN ONCE cagir. */
void renk_hesapla(color **img, int NC, int NR, Bolge *b);

/* Isik cubugunun iki ucu (uzun eksen boyunca). ust = y'si kucuk olan uc. */
void isik_uclari(const Bolge *b, double ust[2], double alt[2]);

/* Isiklari ikiser eslestirip zirhlari bulur. Her isik en fazla bir zirhta kullanilir,
   once en iyi eslesmeler secilir. *liste YENI dizi -> free(*liste). Donus: zirh sayisi. */
int zirh_bul(const Bolge *isiklar, int n, ZirhFiltre f, Zirh **liste);

/* Zirhi cizer: dort kenar + merkezde arti isareti */
void zirh_ciz(color **img, int NC, int NR, const Zirh *z, color c);

#endif /* IRS_VISION_H */
