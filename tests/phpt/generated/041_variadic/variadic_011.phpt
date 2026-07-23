--TEST--
variadic argument collection vector 011
--FILE--
<?php
function generated_variadic_011($base, ...$values) {
    return array_reduce($values, fn($sum, $value) => $sum + $value, $base);
}
echo generated_variadic_011(23, 12, 13, 14, 15);
--EXPECT--
77
