--TEST--
string search and slice vector 016
--FILE--
<?php
$text = 'ggneedle16q';
echo strpos($text, 'needle16'), ':', (str_contains($text, 'needle16') ? 1 : 0), ':', (str_starts_with($text, 'gg') ? 1 : 0), ':', (str_ends_with($text, 'q') ? 1 : 0), ':', substr($text, 2, 8);
--EXPECT--
2:1:1:1:needle16
