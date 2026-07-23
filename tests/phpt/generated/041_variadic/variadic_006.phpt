--TEST--
variadic argument collection vector 006
--FILE--
<?php
function generated_variadic_006($base, ...$values) {
    return array_reduce($values, fn($sum, $value) => $sum + $value, $base);
}
echo generated_variadic_006(13, 7, 8, 9, 10);
--EXPECT--
47
