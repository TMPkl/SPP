### wiem, że nie od tego jest readme - na razie jest tutaj dziennik pokładowy

# ADRESY MAC
dodałem kod, który zapisuje proponowany MAC w pamięci NVS, a przy inicjowaniu po uruchomieniu odczytuje tę wartość i nadpisuje hardware'owy MAC. Zapis do NVS odbywa się tylko raz (patrz zakomentowany kawałek kodu na etapie inicjowania)

dlaczego tak a nie eFuse? Znakomite pytanie, spieszę z odpowiedzią. Jak coś jest oznaczone jako UWAGA MOŻNA WYPALIĆ TYLKO RAZ TYLKO DLA OSÓB CO WIEDZĄ CO ROBIĄ, to ja wiem, że to nie dla mnie

dlaczego zmiana MAC-a? Znakomite pytanie, spieszę z odpowiedzią. Te tanie klony mają często takie same MAC-i jak wychodzą z fabryki. Dodatkowo **zrobiłem tak, że w ostatnim bajcie jest schowany ID, który będzie wykorzystywany do identyfikacji w algorytmie** 

    {0xA0, 0x11, 0x84, 0xAA, 0x2C, **0x03**} 
                                      / \ 
                                       |
                             urządzenie o id 3, dla id 10 będzie tam 0x0A

# secrets.h

plik z oczywistych względów nie jest w repo, to jes ten co wysłałem w konwersacji. Musi znajdować się w `/software_esp/SPP/include/secrets.h`.

# podgląd logów
## na `https://logs.kaleszynski.xyz/` jest podgląd logów na żywo

strasznie dużo gimnastyki devopsowej bo router ma izolowane podsieci mimo opcji _separacja sieci wyłączona_. PRzez to musiało byc tunelowanie - esp robi posta na `logi.kaleszynski.xyz/event` a to tunelem idzie na server. Z uwagi na to że przechodzi to przez ten tunel a ja średnio umiem inaczej to poustawiać to REQUEST musiał być HTTPS, a wieć ESP to musi pakować. Dodałem klucz w secretach aby ktoś przypadkiem nam nie robił syfu. Zużywamy już 45% flasah wiec nie wiem czy nie będzie trzeba zrobić tego prościej.

Logi działaja tak że wysyłają na serial + do servera, potem to zmienię aby wszystko szybiej chodziło.

Jest też podwalina pod OTA, obecnie pod `logi.kaleszynski.xyz/ota` można pobrać jakąś losową binarkę, potem zrobię workflow aby ta z obecnej kompilacji tam leciała. Trzeba też dodać jakiś klucz do teg ota bo po dekomplikacji to widać wszystkie klucze do api I MOJEJ DOMOWEJ SIECI TEŻ.
