--TEST--
string search and slice vector 027
--FILE--
<?php
$text = 'hneedle27rrrr';
echo strpos($text, 'needle27'), ':', (str_contains($text, 'needle27') ? 1 : 0), ':', (str_starts_with($text, 'h') ? 1 : 0), ':', (str_ends_with($text, 'rrrr') ? 1 : 0), ':', substr($text, 1, 8);
--EXPECT--
1:1:1:1:needle27
