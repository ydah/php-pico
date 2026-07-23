--TEST--
string search and slice vector 010
--FILE--
<?php
$text = 'aaneedle10kkk';
echo strpos($text, 'needle10'), ':', (str_contains($text, 'needle10') ? 1 : 0), ':', (str_starts_with($text, 'aa') ? 1 : 0), ':', (str_ends_with($text, 'kkk') ? 1 : 0), ':', substr($text, 2, 8);
--EXPECT--
2:1:1:1:needle10
