--TEST--
string search and slice vector 000
--FILE--
<?php
$text = 'aneedle00k';
echo strpos($text, 'needle00'), ':', (str_contains($text, 'needle00') ? 1 : 0), ':', (str_starts_with($text, 'a') ? 1 : 0), ':', (str_ends_with($text, 'k') ? 1 : 0), ':', substr($text, 1, 8);
--EXPECT--
1:1:1:1:needle00
