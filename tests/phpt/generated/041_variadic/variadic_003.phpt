--TEST--
variadic argument collection vector 003
--FILE--
<?php
function generated_variadic_003($base, ...$values) {
    return array_reduce($values, fn($sum, $value) => $sum + $value, $base);
}
echo generated_variadic_003(7, 4, 5, 6, 7, 8, 9);
--EXPECT--
46
