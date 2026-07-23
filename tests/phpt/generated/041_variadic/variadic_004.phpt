--TEST--
variadic argument collection vector 004
--FILE--
<?php
function generated_variadic_004($base, ...$values) {
    return array_reduce($values, fn($sum, $value) => $sum + $value, $base);
}
echo generated_variadic_004(9, 5, 6, 7, 8, 9, 10, 11);
--EXPECT--
65
