# Lab 5
## Brief
Design, assemble, and test an AC phasor voltmeter using the EFM8 microcontroller
board.

## Requirements
- Two sine waves of the same frequency (about 60Hz); amplitude below maximum V-input of EFM8
- Voltmeter should take the 2 inputs as `reference` and `test` inputs
- Magnitude (In Vrms) and phase (in degrees) displayed on LCD
- _(optional)_ Display frequency of reference signal in Hz 

## To-do
- [ ] Hardware
  - [ ] Precision Peak Detector x 2
    - [ ] Op-amps: `LM324` has 4, `LM358` has 2
    - [ ] Signal diodes e.g. `1N4148`
    - [ ] Resistors
    - [ ] Capacitors
  - [ ] Zero Cross Detector x 2
    - [ ] `LM 339` or `LM 393` comparator
    - 1kΩ resistor
    - Signal diodes e.g. `1N4148`
    - Pull-up resistor
- [ ] Software
  - [ ] ADC
  - [ ] Serial port to PuTTY
  - [ ] Convert peak ADC values to RMS and display
  - [ ] Convert time difference between zero cross of both signals to degrees
  - [ ] Send data to LCD and PuTTY
  
 ## Bonus Ideas
 - [ ] GUI
