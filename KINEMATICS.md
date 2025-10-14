# 🤖 ПРАВИЛЬНЫЕ ФОРМУЛЫ КИНЕМАТИКИ

## ⚠️ ВАЖНО: НЕ ПУТАТЬ!

X-конфигурация омни-робота (вид сверху):
```
    M1 ↗  ↖ M2
        ╲╱
        ╱╲
    M3 ↙  ↘ M4
```

---

## ✅ ПРАВИЛЬНЫЕ ФОРМУЛЫ

### Forward (Вперёд)
```
M1 = +speed
M2 = +speed
M3 = +speed
M4 = +speed
```

### Backward (Назад)
```
M1 = -speed
M2 = -speed
M3 = -speed
M4 = -speed
```

### Strafe Left (Стрейф влево)
```
M1 = -speed
M2 = +speed
M3 = +speed
M4 = -speed
```

### Strafe Right (Стрейф вправо)
```
M1 = +speed
M2 = -speed
M3 = -speed
M4 = +speed
```

### Rotate Left (Поворот влево)
```
M1 = -speed
M2 = +speed
M3 = -speed
M4 = +speed
```

### Rotate Right (Поворот вправо)
```
M1 = +speed
M2 = -speed
M3 = +speed
M4 = -speed
```

---

## 🎮 WIIMOTE-CONTROL BRANCH

**Назначение кнопок (Wiimote горизонтально):**

- **D-pad LEFT (←)**: Forward
- **D-pad RIGHT (→)**: Backward
- **D-pad UP (↑)**: Strafe Left
- **D-pad DOWN (↓)**: Strafe Right
- **Button A / Button 2**: Rotate Right
- **Button B / Button 1**: Rotate Left
- **Button PLUS (+)**: Increase Speed (50-255, step 25)
- **Button MINUS (-)**: Decrease Speed (50-255, step 25)
- **Button HOME**: Emergency Stop

---

## 🌐 WEB-BLUETOOTH-CONTROL BRANCH

**Joystick режимы:**

### Omni Mode (Strafe)
```
M1 = Y + X
M2 = Y - X
M3 = Y + X
M4 = Y - X
```
При X=max → робот стрейфит вправо

### Tank Mode (Rotation)
```
M1 = Y + X
M2 = Y - X
M3 = Y - X
M4 = Y + X
```
При X=max → робот вращается вправо

---

## 📝 ЗАПОМНИТЬ

**Strafe vs Rotation - визуально:**

```
STRAFE LEFT:              STRAFE RIGHT:
M1 ↙  ↖ M2                M1 ↗  ↘ M2
  -     +                   +     -
  +     -                   -     +
M3 ↖  ↙ M4                M3 ↘  ↗ M4

ROTATE LEFT:              ROTATE RIGHT:
M1 ↙  ↖ M2                M1 ↗  ↘ M2
  -     +                   +     -
  -     +                   +     -
M3 ↙  ↖ M4                M3 ↗  ↘ M4
```

**Ключевое отличие:**
- **Strafe**: M1 и M3 вращаются в РАЗНЫХ направлениях (диагональное движение)
- **Rotate**: M1 и M3 вращаются в ОДНОМ направлении (все колёса одной стороны одинаково)
