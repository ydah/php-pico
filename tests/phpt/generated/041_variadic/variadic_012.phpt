--TEST--
variadic argument collection vector 012
--FILE--
<?php
function generated_variadic_012($base, ...$values) {
    return array_reduce($values, fn($sum, $value) => $sum + $value, $base);
}
echo generated_variadic_012(25, 13, 14, 15, 16, 17);
--EXPECT--
100
