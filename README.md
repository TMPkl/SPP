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