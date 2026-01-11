// Во время загрузки
// Дополнительные улучшения UX
// Плавное появление элементов при загрузке
window.addEventListener('load', () => {
    document.querySelectorAll('.smooth-on-load').forEach(el => {
        el.style.opacity = '0';
        el.style.transform = 'translateY(20px)';
        setTimeout(() => {
            el.style.transition = 'opacity 0.6s ease, transform 0.6s ease';
            el.style.opacity = '1';
            el.style.transform = 'translateY(0)';
        }, 100);
    });
});
// class="smooth-on-load"