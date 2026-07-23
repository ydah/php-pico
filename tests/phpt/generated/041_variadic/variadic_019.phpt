--TEST--
variadic argument collection vector 019
--FILE--
<?php
function generated_variadic_019($base, ...$values) {
    return array_reduce($values, fn($sum, $value) => $sum + $value, $base);
}
echo generated_variadic_019(39, 20, 21, 22, 23, 24, 25, 26);
--EXPECT--
200
