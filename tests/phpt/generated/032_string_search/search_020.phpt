--TEST--
string search and slice vector 020
--FILE--
<?php
$text = 'aaaneedle20k';
echo strpos($text, 'needle20'), ':', (str_contains($text, 'needle20') ? 1 : 0), ':', (str_starts_with($text, 'aaa') ? 1 : 0), ':', (str_ends_with($text, 'k') ? 1 : 0), ':', substr($text, 3, 8);
--EXPECT--
3:1:1:1:needle20
