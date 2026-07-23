--TEST--
variadic argument collection vector 013
--FILE--
<?php
function generated_variadic_013($base, ...$values) {
    return array_reduce($values, fn($sum, $value) => $sum + $value, $base);
}
echo generated_variadic_013(27, 14, 15, 16, 17, 18, 19);
--EXPECT--
126
