#include <iostream>
#include "audio/AudioCapture.h"

int main()
{
    std::cout << "=== TEST INIZIALIZZAZIONE PLUG-AND-PLAY ===" << std::endl;

    // Let the class auto-pick the best device
    std::string targetDevice = AudioCapture::getPlugAndPlayDevice();
    std::cout << "[Meteo Audio] Dispositivo selezionato automaticamente: " << targetDevice << std::endl;

    AudioCapture capture;
    if (capture.openDevice(targetDevice))
    {
        std::cout << "\n[TEST SUCCESS] Dispositivo configurato con successo!" << std::endl;
        capture.closeDevice();
    }
    else
    {
        std::cout << "\n[TEST FAILED] Impossibile configurare il dispositivo." << std::endl;
        return -1;
    }

    return 0;
}
