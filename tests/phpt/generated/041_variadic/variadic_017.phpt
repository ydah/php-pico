--TEST--
variadic argument collection vector 017
--FILE--
<?php
function generated_variadic_017($base, ...$values) {
    return array_reduce($values, fn($sum, $value) => $sum + $value, $base);
}
echo generated_variadic_017(35, 18, 19, 20, 21, 22);
--EXPECT--
135
