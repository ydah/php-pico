--TEST--
variadic argument collection vector 008
--FILE--
<?php
function generated_variadic_008($base, ...$values) {
    return array_reduce($values, fn($sum, $value) => $sum + $value, $base);
}
echo generated_variadic_008(17, 9, 10, 11, 12, 13, 14);
--EXPECT--
86
