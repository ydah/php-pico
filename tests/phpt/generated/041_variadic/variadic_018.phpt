--TEST--
variadic argument collection vector 018
--FILE--
<?php
function generated_variadic_018($base, ...$values) {
    return array_reduce($values, fn($sum, $value) => $sum + $value, $base);
}
echo generated_variadic_018(37, 19, 20, 21, 22, 23, 24);
--EXPECT--
166
