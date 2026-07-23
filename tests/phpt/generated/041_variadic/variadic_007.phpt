--TEST--
variadic argument collection vector 007
--FILE--
<?php
function generated_variadic_007($base, ...$values) {
    return array_reduce($values, fn($sum, $value) => $sum + $value, $base);
}
echo generated_variadic_007(15, 8, 9, 10, 11, 12);
--EXPECT--
65
