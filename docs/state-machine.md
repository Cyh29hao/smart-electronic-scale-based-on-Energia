# Smart Electronic Scale State Machine / 智能电子秤状态机

The public diagram below is reconstructed from `SmartElectronicScale/SmartElectronicScale.ino` so the repository can explain the workflow without publishing the original local diagram that contained personal metadata.

```mermaid
stateDiagram-v2
    [*] --> Welcome
    Welcome: State 1\nWelcome prompt
    DefaultCalibration: State 2\n0 g and 50 g calibration
    Weighing: State 0\nPrint filtered weight\n3 significant digits
    Tare: State 3\nSet current fitted weight to zero
    AdvancedCalibration: State 4\n0 g, 10 g, 20 g, 50 g fit
    UnitSwitch: State 5\nCycle g / kg / oz / lb

    Welcome --> DefaultCalibration: PUSH1 release
    DefaultCalibration --> Weighing: calibration complete
    Weighing --> Tare: PUSH1 release
    Tare --> Weighing: offset updated
    Weighing --> AdvancedCalibration: hold PUSH2 for 3 s
    AdvancedCalibration --> Weighing: fit complete
    Weighing --> UnitSwitch: short PUSH2 release
    UnitSwitch --> Weighing: unit updated
    Weighing --> Weighing: 250 ms print interval
```

## Interaction Summary

| Trigger | Result |
| --- | --- |
| Startup | Print welcome text, then wait for `PUSH1` release |
| Default calibration | Read empty scale and 50 g reference, then fit `y = kx + b` |
| `PUSH1` release during weighing | Tare / recalibrate the zero offset |
| `PUSH2` held for 3 s | Enter advanced calibration with 0 g, 10 g, 20 g, and 50 g references |
| Short `PUSH2` release | Switch display unit among `g`, `kg`, `oz`, and `lb` |
| 250 ms timer interval | Print the latest filtered weight over serial |
